// DynSoA Runtime SDK

#pragma once
#include <cstdint>
#include <cstddef>

namespace dynsoa {

using ArchetypeId = std::uint64_t;
using ViewId      = std::uint64_t;

enum class Device : std::uint8_t { CPU = 0, GPU = 1 };
enum class ScalarType : std::uint8_t { F32=0, I32=1, U32=2, F64=3, I64=4 };

struct Config {
  Device device = Device::CPU;
  int aosoa_tile = 128;
  int matrix_block = 1024;
  int max_retile_us = 500;
  bool scheduler_enabled = false;
};

struct Field { const char* name; ScalarType type; };

struct Component {
  const char*  name;
  const Field* fields;
  int          field_count;
};

struct KernelCtx { float dt; int tile; };

struct MatrixBlock {
  float* data = nullptr;  // column-major (M[j*rows + i])
  int    rows = 0;        // B
  int    cols = 0;        // K
  int    leading_dim = 0; // rows == leading_dim
  std::size_t bytes = 0;
  std::size_t offset = 0;
};

struct FrameAgg {
  double mean_us=0, p95_us=0, p99_us=0;
  double warp_eff=1, branch_div=0, mem_coalesce=1, l2_miss=0;
  double tail_ratio=0; // p99/p95
};

} // namespace dynsoa
