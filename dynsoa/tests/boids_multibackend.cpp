// DynSoA Runtime SDK
// (C) 2025 Sungmin "Jason" Jun
//
// Covered under:
//  - U.S. Patent Application No. 19/303,020
//  - U.S. Provisional No. 63/775,990
//  - CIP: Systems and Methods for Adaptive Optimization and Coordination of Data Layout and Execution
//
// Licensed under the Mozilla Public License 2.0 (MPL 2.0).
// Commercial use requires a separate commercial license.
// Unauthorized commercial use may infringe one or more patents.

#include "dynsoa/dynsoa.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace dynsoa;

// =========================
// Common helpers
// =========================

constexpr int MAX_NEIGHBORS = 64;

enum BehaviorFlags : std::uint32_t {
  BEHAVIOR_NONE        = 0,
  BEHAVIOR_AVOID       = 1 << 0,
  BEHAVIOR_ALIGN       = 1 << 1,
  BEHAVIOR_COHERE      = 1 << 2,
  BEHAVIOR_HIGH_ENERGY = 1 << 3
};

static int env_int(const char* name, int fallback) {
  if (const char* v = std::getenv(name)) return std::atoi(v);
  return fallback;
}

static long long env_ll(const char* name, long long fallback) {
  if (const char* v = std::getenv(name)) return std::atoll(v);
  return fallback;
}

struct BoidsParams {
  float dt               = 0.016f; // ~60 FPS
  float neighbor_radius  = 3.0f;
  float separation_radius= 1.0f;
  float separation_weight= 1.5f;
  float alignment_weight = 1.0f;
  float cohesion_weight  = 1.0f;
  float max_speed        = 10.0f;
  float world_half_extent= 100.0f;
};

// Simple CSV writer with backend field
class CsvWriter {
public:
  explicit CsvWriter(const char* path)
    : out_(path, std::ios::out | std::ios::trunc) {
    if (out_) {
      out_ << "backend,frame,num_entities,ms\n";
    }
  }

  void write_row(const std::string& backend,
                 int frame,
                 std::size_t num_entities,
                 double ms) {
    if (!out_) return;
    out_ << backend << ","
         << frame << ","
         << num_entities << ","
         << ms << "\n";
  }

private:
  std::ofstream out_;
};

// =========================
// OOP backend
// =========================

struct Vec3 {
  float x, y, z;
};

static float length_sq(const Vec3& v) {
  return v.x*v.x + v.y*v.y + v.z*v.z;
}

struct EntityOOP {
  Vec3 position;
  Vec3 velocity;
  std::uint32_t flags;
};

static void init_oop(std::vector<EntityOOP>& ents,
                     std::size_t N,
                     const BoidsParams& params,
                     unsigned int seed) {
  ents.clear();
  ents.resize(N);

  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> posd(-params.world_half_extent,
                                             params.world_half_extent);
  std::uniform_real_distribution<float> veld(-1.0f, 1.0f);
  std::uniform_int_distribution<std::uint32_t> bitsd;

  for (auto& e : ents) {
    e.position = Vec3{posd(rng), posd(rng), posd(rng)};
    e.velocity = Vec3{veld(rng), veld(rng), veld(rng)};

    std::uint32_t bits = bitsd(rng);
    std::uint32_t f = 0;
    if (bits & 1) f |= BEHAVIOR_AVOID;
    if (bits & 2) f |= BEHAVIOR_ALIGN;
    if (bits & 4) f |= BEHAVIOR_COHERE;
    if (bits & 8) f |= BEHAVIOR_HIGH_ENERGY;
    e.flags = f;
  }
}

static void step_oop(std::vector<EntityOOP>& ents,
                     const BoidsParams& params) {
  std::size_t N = ents.size();
  float neighbor_r2   = params.neighbor_radius   * params.neighbor_radius;
  float separation_r2 = params.separation_radius * params.separation_radius;
  float dt            = params.dt;
  float max_speed2    = params.max_speed * params.max_speed;

  // compute accelerations
  std::vector<Vec3> accel(N, Vec3{0,0,0});

  for (std::size_t i = 0; i < N; ++i) {
    const auto& self = ents[i];
    float px = self.position.x;
    float py = self.position.y;
    float pz = self.position.z;
    std::uint32_t f = self.flags;

    Vec3 sep{0,0,0}, ali{0,0,0}, coh{0,0,0};
    int count = 0;

    for (std::size_t j = 0; j < N; ++j) {
      if (j == i) continue;
      const auto& other = ents[j];
      float dx = other.position.x - px;
      float dy = other.position.y - py;
      float dz = other.position.z - pz;
      float dist2 = dx*dx + dy*dy + dz*dz;
      if (dist2 > neighbor_r2) continue;
      ++count;

      if (f & BEHAVIOR_AVOID) {
        if (dist2 < separation_r2) {
          sep.x -= dx; sep.y -= dy; sep.z -= dz;
        }
      }

      if (f & BEHAVIOR_ALIGN) {
        ali.x += other.velocity.x;
        ali.y += other.velocity.y;
        ali.z += other.velocity.z;
      }

      if (f & BEHAVIOR_COHERE) {
        coh.x += other.position.x;
        coh.y += other.position.y;
        coh.z += other.position.z;
      }
    }

    Vec3 a{0,0,0};
    if (count > 0) {
      if (f & BEHAVIOR_ALIGN) {
        ali.x /= count; ali.y /= count; ali.z /= count;
        a.x += ali.x * params.alignment_weight;
        a.y += ali.y * params.alignment_weight;
        a.z += ali.z * params.alignment_weight;
      }
      if (f & BEHAVIOR_COHERE) {
        coh.x /= count; coh.y /= count; coh.z /= count;
        float tcx = coh.x - px;
        float tcy = coh.y - py;
        float tcz = coh.z - pz;
        a.x += tcx * params.cohesion_weight;
        a.y += tcy * params.cohesion_weight;
        a.z += tcz * params.cohesion_weight;
      }
      if (f & BEHAVIOR_AVOID) {
        a.x += sep.x * params.separation_weight;
        a.y += sep.y * params.separation_weight;
        a.z += sep.z * params.separation_weight;
      }
    }

    if (f & BEHAVIOR_HIGH_ENERGY) {
      a.x *= 1.5f; a.y *= 1.5f; a.z *= 1.5f;
    }
    accel[i] = a;
  }

  // integrate
  for (std::size_t i = 0; i < N; ++i) {
    auto& e = ents[i];
    e.velocity.x += accel[i].x * dt;
    e.velocity.y += accel[i].y * dt;
    e.velocity.z += accel[i].z * dt;

    float s2 = length_sq(e.velocity);
    if (s2 > max_speed2) {
      float inv_len = 1.0f / std::sqrt(s2);
      e.velocity.x *= params.max_speed * inv_len;
      e.velocity.y *= params.max_speed * inv_len;
      e.velocity.z *= params.max_speed * inv_len;
    }

    e.position.x += e.velocity.x * dt;
    e.position.y += e.velocity.y * dt;
    e.position.z += e.velocity.z * dt;

    float w = params.world_half_extent;
    if (e.position.x < -w) e.position.x += 2*w; else if (e.position.x > w) e.position.x -= 2*w;
    if (e.position.y < -w) e.position.y += 2*w; else if (e.position.y > w) e.position.y -= 2*w;
    if (e.position.z < -w) e.position.z += 2*w; else if (e.position.z > w) e.position.z -= 2*w;
  }
}

static void run_oop_backend(CsvWriter& writer,
                            std::size_t num_entities,
                            int frames,
                            const BoidsParams& params) {
  std::vector<EntityOOP> ents;
  init_oop(ents, num_entities, params, /*seed=*/12345);

  for (int f = 0; f < frames; ++f) {
    auto t0 = std::chrono::high_resolution_clock::now();
    step_oop(ents, params);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    writer.write_row("OOP", f, num_entities, ms);
  }
  std::printf("OOP backend done.\n");
}

// =========================
// Static SoA backend
// =========================

struct SoABoids {
  std::vector<float> px, py, pz;
  std::vector<float> vx, vy, vz;
  std::vector<std::uint32_t> flags;
};

static void init_soa(SoABoids& b,
                     std::size_t N,
                     const BoidsParams& params,
                     unsigned int seed) {
  b.px.resize(N); b.py.resize(N); b.pz.resize(N);
  b.vx.resize(N); b.vy.resize(N); b.vz.resize(N);
  b.flags.resize(N);

  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> posd(-params.world_half_extent,
                                             params.world_half_extent);
  std::uniform_real_distribution<float> veld(-1.0f, 1.0f);
  std::uniform_int_distribution<std::uint32_t> bitsd;

  for (std::size_t i = 0; i < N; ++i) {
    b.px[i] = posd(rng);
    b.py[i] = posd(rng);
    b.pz[i] = posd(rng);

    b.vx[i] = veld(rng);
    b.vy[i] = veld(rng);
    b.vz[i] = veld(rng);

    std::uint32_t bits = bitsd(rng);
    std::uint32_t f = 0;
    if (bits & 1) f |= BEHAVIOR_AVOID;
    if (bits & 2) f |= BEHAVIOR_ALIGN;
    if (bits & 4) f |= BEHAVIOR_COHERE;
    if (bits & 8) f |= BEHAVIOR_HIGH_ENERGY;
    b.flags[i] = f;
  }
}

static void step_soa(SoABoids& b,
                     const BoidsParams& params) {
  std::size_t N = b.px.size();
  float neighbor_r2   = params.neighbor_radius   * params.neighbor_radius;
  float separation_r2 = params.separation_radius * params.separation_radius;
  float dt            = params.dt;
  float max_speed2    = params.max_speed * params.max_speed;

  std::vector<float> ax(N,0.0f), ay(N,0.0f), az(N,0.0f);

  for (std::size_t i = 0; i < N; ++i) {
    float px_i = b.px[i];
    float py_i = b.py[i];
    float pz_i = b.pz[i];
    std::uint32_t f = b.flags[i];

    float sep_x=0, sep_y=0, sep_z=0;
    float ali_x=0, ali_y=0, ali_z=0;
    float coh_x=0, coh_y=0, coh_z=0;
    int count = 0;

    for (std::size_t j = 0; j < N; ++j) {
      if (j == i) continue;
      float dx = b.px[j] - px_i;
      float dy = b.py[j] - py_i;
      float dz = b.pz[j] - pz_i;
      float dist2 = dx*dx + dy*dy + dz*dz;
      if (dist2 > neighbor_r2) continue;
      ++count;

      if (f & BEHAVIOR_AVOID) {
        if (dist2 < separation_r2) {
          sep_x -= dx; sep_y -= dy; sep_z -= dz;
        }
      }
      if (f & BEHAVIOR_ALIGN) {
        ali_x += b.vx[j]; ali_y += b.vy[j]; ali_z += b.vz[j];
      }
      if (f & BEHAVIOR_COHERE) {
        coh_x += b.px[j]; coh_y += b.py[j]; coh_z += b.pz[j];
      }
    }

    float Ax=0, Ay=0, Az=0;
    if (count > 0) {
      if (f & BEHAVIOR_ALIGN) {
        ali_x /= count; ali_y /= count; ali_z /= count;
        Ax += ali_x * params.alignment_weight;
        Ay += ali_y * params.alignment_weight;
        Az += ali_z * params.alignment_weight;
      }
      if (f & BEHAVIOR_COHERE) {
        coh_x /= count; coh_y /= count; coh_z /= count;
        float tcx = coh_x - px_i;
        float tcy = coh_y - py_i;
        float tcz = coh_z - pz_i;
        Ax += tcx * params.cohesion_weight;
        Ay += tcy * params.cohesion_weight;
        Az += tcz * params.cohesion_weight;
      }
      if (f & BEHAVIOR_AVOID) {
        Ax += sep_x * params.separation_weight;
        Ay += sep_y * params.separation_weight;
        Az += sep_z * params.separation_weight;
      }
    }
    if (f & BEHAVIOR_HIGH_ENERGY) {
      Ax *= 1.5f; Ay *= 1.5f; Az *= 1.5f;
    }

    ax[i] = Ax; ay[i] = Ay; az[i] = Az;
  }

  for (std::size_t i = 0; i < N; ++i) {
    float vx_i = b.vx[i] + ax[i]*dt;
    float vy_i = b.vy[i] + ay[i]*dt;
    float vz_i = b.vz[i] + az[i]*dt;
    float s2 = vx_i*vx_i + vy_i*vy_i + vz_i*vz_i;
    if (s2 > max_speed2) {
      float inv_len = 1.0f / std::sqrt(s2);
      vx_i *= params.max_speed * inv_len;
      vy_i *= params.max_speed * inv_len;
      vz_i *= params.max_speed * inv_len;
    }
    b.vx[i] = vx_i;
    b.vy[i] = vy_i;
    b.vz[i] = vz_i;

    b.px[i] += vx_i * dt;
    b.py[i] += vy_i * dt;
    b.pz[i] += vz_i * dt;

    float w = params.world_half_extent;
    if (b.px[i] < -w) b.px[i] += 2*w; else if (b.px[i] > w) b.px[i] -= 2*w;
    if (b.py[i] < -w) b.py[i] += 2*w; else if (b.py[i] > w) b.py[i] -= 2*w;
    if (b.pz[i] < -w) b.pz[i] += 2*w; else if (b.pz[i] > w) b.pz[i] -= 2*w;
  }
}

static void run_soa_backend(CsvWriter& writer,
                            std::size_t num_entities,
                            int frames,
                            const BoidsParams& params) {
  SoABoids b;
  init_soa(b, num_entities, params, /*seed=*/12345);

  for (int f = 0; f < frames; ++f) {
    auto t0 = std::chrono::high_resolution_clock::now();
    step_soa(b, params);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    writer.write_row("SoA", f, num_entities, ms);
  }
  std::printf("SoA backend done.\n");
}

// =========================
// DynSoA backend
// =========================

static void boids_kernel_dynsoa(ViewId v, const KernelCtx& ctx) {
  int n = (int)view_len(v);
  if (n <= 0) return;

  float* px = (float*)column(v, "Position.x");
  float* py = (float*)column(v, "Position.y");
  float* pz = (float*)column(v, "Position.z");

  float* vx = (float*)column(v, "Velocity.vx");
  float* vy = (float*)column(v, "Velocity.vy");
  float* vz = (float*)column(v, "Velocity.vz");

  std::uint32_t* flags = (std::uint32_t*)column(v, "Flags.mask");
  if (!px || !py || !pz || !vx || !vy || !vz || !flags) return;

  const float dt               = ctx.dt;
  const float neighbor_radius  = 3.0f;
  const float neighbor_r2      = neighbor_radius * neighbor_radius;
  const float separation_radius= 1.0f;
  const float separation_r2    = separation_radius * separation_radius;

  const float separation_weight= 1.5f;
  const float alignment_weight = 1.0f;
  const float cohesion_weight  = 1.0f;

  const float max_speed        = 10.0f;
  const float max_speed2       = max_speed * max_speed;

  for (int i = 0; i < n; ++i) {
    float px_i = px[i], py_i = py[i], pz_i = pz[i];
    std::uint32_t f = flags[i];

    float sep_x=0, sep_y=0, sep_z=0;
    float ali_x=0, ali_y=0, ali_z=0;
    float coh_x=0, coh_y=0, coh_z=0;
    int count = 0;

    for (int j = 0; j < n; ++j) {
      if (j == i) continue;
      float dx = px[j] - px_i;
      float dy = py[j] - py_i;
      float dz = pz[j] - pz_i;
      float dist2 = dx*dx + dy*dy + dz*dz;
      if (dist2 > neighbor_r2) continue;
      ++count;

      if (f & BEHAVIOR_AVOID) {
        if (dist2 < separation_r2) {
          sep_x -= dx; sep_y -= dy; sep_z -= dz;
        }
      }
      if (f & BEHAVIOR_ALIGN) {
        ali_x += vx[j]; ali_y += vy[j]; ali_z += vz[j];
      }
      if (f & BEHAVIOR_COHERE) {
        coh_x += px[j]; coh_y += py[j]; coh_z += pz[j];
      }
    }

    float ax=0, ay=0, az=0;
    if (count > 0) {
      if (f & BEHAVIOR_ALIGN) {
        ali_x /= count; ali_y /= count; ali_z /= count;
        ax += ali_x * alignment_weight;
        ay += ali_y * alignment_weight;
        az += ali_z * alignment_weight;
      }
      if (f & BEHAVIOR_COHERE) {
        coh_x /= count; coh_y /= count; coh_z /= count;
        float tcx = coh_x - px_i;
        float tcy = coh_y - py_i;
        float tcz = coh_z - pz_i;
        ax += tcx * cohesion_weight;
        ay += tcy * cohesion_weight;
        az += tcz * cohesion_weight;
      }
      if (f & BEHAVIOR_AVOID) {
        ax += sep_x * separation_weight;
        ay += sep_y * separation_weight;
        az += sep_z * separation_weight;
      }
    }

    if (f & BEHAVIOR_HIGH_ENERGY) {
      ax *= 1.5f; ay *= 1.5f; az *= 1.5f;
    }

    float vx_i = vx[i] + ax * dt;
    float vy_i = vy[i] + ay * dt;
    float vz_i = vz[i] + az * dt;
    float s2 = vx_i*vx_i + vy_i*vy_i + vz_i*vz_i;
    if (s2 > max_speed2) {
      float inv_len = 1.0f / std::sqrt(s2);
      vx_i *= max_speed * inv_len;
      vy_i *= max_speed * inv_len;
      vz_i *= max_speed * inv_len;
    }

    vx[i] = vx_i;
    vy[i] = vy_i;
    vz[i] = vz_i;

    px[i] = px_i + vx_i * dt;
    py[i] = py_i + vy_i * dt;
    pz[i] = pz_i + vz_i * dt;
  }
}

static void run_dynsoa_backend(CsvWriter& writer,
                               std::size_t num_entities,
                               int frames,
                               const BoidsParams& params) {
  Config cfg;
  cfg.scheduler_enabled = true;
  cfg.max_retile_us     = 500;

  dynsoa_init(&cfg);

  Field pos_fields[] = {
    {"x", ScalarType::F32},
    {"y", ScalarType::F32},
    {"z", ScalarType::F32}
  };
  Field vel_fields[] = {
    {"vx", ScalarType::F32},
    {"vy", ScalarType::F32},
    {"vz", ScalarType::F32}
  };
  Field flag_fields[] = {
    {"mask", ScalarType::U32}
  };

  Component Position{"Position", pos_fields, 3};
  Component Velocity{"Velocity", vel_fields, 3};
  Component Flags   {"Flags",    flag_fields, 1};

  dynsoa_define_component(&Position);
  dynsoa_define_component(&Velocity);
  dynsoa_define_component(&Flags);

  const char* comps[] = {"Position","Velocity","Flags"};
  ArchetypeId arch = dynsoa_define_archetype("Boid", comps, 3);

  dynsoa_spawn(arch, num_entities, nullptr);
  ViewId view = dynsoa_make_view(arch);

  // internal metrics CSV if you want it
  dynsoa_metrics_enable_csv("metrics_internal_dynsoa.csv");
  dynsoa_set_policy("{}");

  KernelCtx ctx{params.dt, cfg.aosoa_tile};

  for (int f = 0; f < frames; ++f) {
    auto t0 = std::chrono::high_resolution_clock::now();
    dynsoa_begin_frame();
    dynsoa_run_kernel("boids_step", boids_kernel_dynsoa, view, &ctx);
    dynsoa_end_frame();
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    writer.write_row("DynSoA", f, num_entities, ms);
  }

  std::printf("DynSoA backend done.\n");

  dynsoa_shutdown();
}

// =========================
// main()
// =========================

int main() {
  std::size_t num_entities = (std::size_t)env_ll("DYNSOA_ENTITIES", 200000); // you can bump to 500k/1M
  int frames               = env_int("DYNSOA_FRAMES", 300);

  BoidsParams params; // defaults okay for now

  CsvWriter writer("bench.csv");

  std::printf("Running OOP backend...\n");
  run_oop_backend(writer, num_entities, frames, params);

  std::printf("Running SoA backend...\n");
  run_soa_backend(writer, num_entities, frames, params);

  std::printf("Running DynSoA backend...\n");
  run_dynsoa_backend(writer, num_entities, frames, params);

  std::printf("All backends done. Wrote bench.csv\n");
  return 0;
}
