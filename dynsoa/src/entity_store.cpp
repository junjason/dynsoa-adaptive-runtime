// DynSoA Runtime SDK

#include "dynsoa/entity_store.h"
#include "dynsoa/schema.h"
#include "dynsoa/layout.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace dynsoa {

struct ColumnData {
  std::vector<std::uint8_t> bytes;
  std::size_t elem_size = sizeof(float); // default f32 for sample
};

struct ViewRec {
  ArchetypeId arch{};
  std::size_t len{};
  std::unordered_map<std::string, ColumnData> columns;
  LayoutKind layout = LayoutKind::SoA;
  int aosoa_tile = 0;
};

std::vector<ViewRec> g_views;

void* spawn(ArchetypeId arch, std::size_t count, void(*init_fn)(std::size_t, void*)) {
  ViewRec v; v.arch = arch; v.len = count;

  auto makeF32Col = [&](const std::string& path){
    ColumnData cd; cd.elem_size = sizeof(float);
    cd.bytes.resize(count * cd.elem_size);
    v.columns[path] = std::move(cd);
  };
  makeF32Col("Position.x"); makeF32Col("Position.y"); makeF32Col("Position.z");
  makeF32Col("Velocity.vx"); makeF32Col("Velocity.vy"); makeF32Col("Velocity.vz");

  if (init_fn) {
    struct Row { float px,py,pz,vx,vy,vz; } row{};
    for (std::size_t i=0;i<count;++i) { init_fn(i, (void*)&row); }
  }

  g_views.push_back(std::move(v));
  return nullptr;
}

ViewId make_view(ArchetypeId arch) {
  for (std::size_t i=0;i<g_views.size();++i)
    if (g_views[i].arch == arch) return static_cast<ViewId>(i+1);
  g_views.push_back(ViewRec{arch,0,{},LayoutKind::SoA,0});
  return static_cast<ViewId>(g_views.size());
}

size_t view_len(ViewId v) {
  auto idx = static_cast<std::size_t>(v-1);
  return g_views[idx].len;
}

void* column(ViewId v, const char* path) {
  auto idx = static_cast<std::size_t>(v-1);
  auto it = g_views[idx].columns.find(path);
  if (it == g_views[idx].columns.end()) return nullptr;
  return (void*)it->second.bytes.data();
}

MatrixBlock acquire_matrix_block(ViewId v, const char** comps, int K, int B, std::size_t offset) {
  auto& V = g_views[(std::size_t)v-1];
  MatrixBlock mb; mb.rows = B; mb.cols = K; mb.leading_dim = B; mb.offset = offset;
  mb.bytes = sizeof(float) * (std::size_t)B * (std::size_t)K;
  mb.data  = (float*)std::malloc(mb.bytes);
  for (int j=0; j<K; ++j) {
    auto it = V.columns.find(comps[j]);
    if (it == V.columns.end()) continue;
    float* src = (float*)it->second.bytes.data();
    for (int i=0; i<B; ++i) {
      std::size_t idx = offset + (std::size_t)i;
      if (idx >= V.len) break;
      mb.data[j*B + i] = src[idx];
    }
  }
  return mb;
}

void release_matrix_block(ViewId v, MatrixBlock* mb, bool write_back) {
  if (!mb) return;
  if (write_back && mb->data) {
    auto& V = g_views[(std::size_t)v-1];
    int K = mb->cols;
    int B = mb->rows;
    int j = 0;
    for (auto& kv : V.columns) {
      if (j >= K) break;
      float* dst = (float*)kv.second.bytes.data();
      for (int i=0; i<B; ++i) {
        std::size_t idx = mb->offset + (std::size_t)i;
        if (idx >= V.len) break;
        dst[idx] = mb->data[j*B + i];
      }
      ++j;
    }
  }
  if (mb->data) std::free(mb->data);
  *mb = {};
}


std::size_t bytes_to_move(ViewId v) {
  auto& V = g_views[(std::size_t)v-1];
  std::size_t sum = 0;
  for (auto& kv : V.columns) sum += kv.second.bytes.size();
  return sum;
}

LayoutKind entity_current_layout(ViewId v) {
  auto& V = g_views[(std::size_t)v-1];
  return V.layout;
}

void transform_soa_to_aosoa(ViewId v, int T) {
  auto& V = g_views[(std::size_t)v-1];
  const std::size_t N = V.len;
  const std::size_t tile_cnt = (N + T - 1) / T;

  for (auto& kv : V.columns) {
    auto& col = kv.second;
    const std::size_t elem = col.elem_size;
    const std::uint8_t* src = col.bytes.data();
    std::vector<std::uint8_t> dst(col.bytes.size());

    for (std::size_t k = 0; k < tile_cnt; ++k) {
      std::size_t b = k * T;
      std::size_t e = std::min(N, b + (std::size_t)T);
      std::size_t n = e - b;
      std::memcpy(dst.data() + b*elem, src + b*elem, n*elem);
    }
    col.bytes.swap(dst);
  }
  V.layout = LayoutKind::AoSoA;
  V.aosoa_tile = T;
}

void transform_aosoa_to_soa(ViewId v) {
  auto& V = g_views[(std::size_t)v-1];
  if (V.layout != LayoutKind::AoSoA) { V.layout = LayoutKind::SoA; V.aosoa_tile = 0; return; }
  for (auto& kv : V.columns) {
    auto& col = kv.second;
    std::vector<std::uint8_t> dst(col.bytes.size());
    std::memcpy(dst.data(), col.bytes.data(), col.bytes.size());
    col.bytes.swap(dst);
  }
  V.layout = LayoutKind::SoA; V.aosoa_tile = 0;
}

} // namespace dynsoa
