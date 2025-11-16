// DynSoA Runtime SDK

#include "dynsoa/dynsoa.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <fstream>
#include <string>

// We’ll use the C++ namespace-level helpers (view_len, column, current_layout, etc.)
using namespace dynsoa;

// -----------------------------------------------------
// Behavior flags to induce branch divergence
// -----------------------------------------------------
enum BehaviorFlags : std::uint32_t {
  BEHAVIOR_NONE        = 0,
  BEHAVIOR_AVOID       = 1 << 0,
  BEHAVIOR_ALIGN       = 1 << 1,
  BEHAVIOR_COHERE      = 1 << 2,
  BEHAVIOR_HIGH_ENERGY = 1 << 3
};

// -----------------------------------------------------
// Env helpers
// -----------------------------------------------------
static int env_int(const char* name, int fallback) {
  if (const char* v = std::getenv(name)) return std::atoi(v);
  return fallback;
}

static long long env_ll(const char* name, long long fallback) {
  if (const char* v = std::getenv(name)) return std::atoll(v);
  return fallback;
}

// -----------------------------------------------------
// CSV writer for a useful bench.csv
// -----------------------------------------------------
class CsvWriter {
public:
  explicit CsvWriter(const char* path) : out_(path, std::ios::out | std::ios::trunc) {
    if (out_) {
      out_ << "backend,frame,num_entities,ms,layout_before,layout_after,retile\n";
    }
  }

  void write_row(const std::string& backend,
                 int frame,
                 std::size_t num_entities,
                 double ms,
                 int layout_before,
                 int layout_after,
                 int retile_flag) {
    if (!out_) return;
    out_ << backend << ","
         << frame << ","
         << num_entities << ","
         << ms << ","
         << layout_before << ","
         << layout_after << ","
         << retile_flag << "\n";
  }

private:
  std::ofstream out_;
};

// -----------------------------------------------------
// Boids kernel running *inside* DynSoA
// -----------------------------------------------------
static void boids_kernel(ViewId v, const KernelCtx& ctx) {
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
  const float separation_radius = 1.0f;
  const float separation_r2     = separation_radius * separation_radius;

  const float separation_weight = 1.5f;
  const float alignment_weight  = 1.0f;
  const float cohesion_weight   = 1.0f;

  const float max_speed  = 10.0f;
  const float max_speed2 = max_speed * max_speed;

  // Naive O(N^2) + branching → intentionally heavy/divergent to exercise DynSoA.
  for (int i = 0; i < n; ++i) {
    float px_i = px[i];
    float py_i = py[i];
    float pz_i = pz[i];
    std::uint32_t f = flags[i];

    float sep_x = 0, sep_y = 0, sep_z = 0;
    float ali_x = 0, ali_y = 0, ali_z = 0;
    float coh_x = 0, coh_y = 0, coh_z = 0;
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

    float ax = 0, ay = 0, az = 0;

    if (count > 0) {
      if (f & BEHAVIOR_ALIGN) {
        ali_x /= count; ali_y /= count; ali_z /= count;
        ax += ali_x * alignment_weight;
        ay += ali_y * alignment_weight;
        az += ali_z * alignment_weight;
      }

      if (f & BEHAVIOR_COHERE) {
        coh_x /= count; coh_y /= count; coh_z /= count;
        float to_cx = coh_x - px_i;
        float to_cy = coh_y - py_i;
        float to_cz = coh_z - pz_i;
        ax += to_cx * cohesion_weight;
        ay += to_cy * cohesion_weight;
        az += to_cz * cohesion_weight;
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

    // Integrate velocity
    float vx_i = vx[i] + ax * dt;
    float vy_i = vy[i] + ay * dt;
    float vz_i = vz[i] + az * dt;

    float speed2 = vx_i*vx_i + vy_i*vy_i + vz_i*vz_i;
    if (speed2 > max_speed2) {
      float inv_len = 1.0f / std::sqrt(speed2);
      vx_i = vx_i * max_speed * inv_len;
      vy_i = vy_i * max_speed * inv_len;
      vz_i = vz_i * max_speed * inv_len;
    }

    vx[i] = vx_i;
    vy[i] = vy_i;
    vz[i] = vz_i;

    // Integrate position
    px[i] = px_i + vx_i * dt;
    py[i] = py_i + vy_i * dt;
    pz[i] = pz_i + vz_i * dt;
  }
}

// -----------------------------------------------------
// main()
// -----------------------------------------------------
int main() {
  // ---------- Config / init ----------
  Config cfg;
  cfg.scheduler_enabled = true;
  cfg.max_retile_us     = 500;    // allow some retile budget
  // cfg.device, cfg.aosoa_tile, etc. can stay defaults for now.

  dynsoa_init(&cfg);

  // ---------- Schema: Position, Velocity, Flags ----------
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

  const char* comps[] = {"Position", "Velocity", "Flags"};
  ArchetypeId arch    = dynsoa_define_archetype("Boid", comps, 3);

  // ---------- Spawn entities ----------
  const long long default_entities = 500000;
  std::size_t num_entities = (std::size_t)env_ll("DYNSOA_ENTITIES", default_entities);

  dynsoa_spawn(arch, num_entities, nullptr);
  ViewId view = dynsoa_make_view(arch);

  // ---------- Optional: keep internal metrics CSV separate ----------
  // This is the low-level internal metrics (time_us, etc.) from dynsoa itself.
  dynsoa_metrics_enable_csv("metrics_internal.csv");

  // Simple always-trigger policy (like your runtime demo)
  dynsoa_set_policy("{}");

  // ---------- Simulation params ----------
  int frames  = env_int("DYNSOA_FRAMES", 1000);
  float dt    = 0.016f;             // ~60 FPS
  KernelCtx ctx{dt, cfg.aosoa_tile};

  CsvWriter writer("bench.csv");

  // ---------- Main loop with timing + layout tracking ----------
  for (int f = 0; f < frames; ++f) {
    // Layout before frame (AoS/SoA/AoSoA/Matrix)
    LayoutKind layout_before = current_layout(view);

    auto t0 = std::chrono::high_resolution_clock::now();
    dynsoa_begin_frame();
    dynsoa_run_kernel("boids_step", boids_kernel, view, &ctx);
    dynsoa_end_frame();
    auto t1 = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    LayoutKind layout_after = current_layout(view);
    int retile_flag = (layout_after != layout_before) ? 1 : 0;

    writer.write_row(
      "DynSoA",
      f,
      num_entities,
      ms,
      static_cast<int>(layout_before),
      static_cast<int>(layout_after),
      retile_flag
    );
  }

  // ---------- Optional: exercise AoSoA + matrix block APIs ----------
  dynsoa_retile_aosoa_plan_apply(view, 128);
  const char* cols[] = {"Position.x", "Velocity.vx"};
  MatrixBlock mb{};
  dynsoa_acquire_matrix_block(view, cols, 2, 1024, 0, &mb);
  dynsoa_release_matrix_block(view, &mb, 0);

  std::printf("OK: ran boids_step on %zu entities for %d frames\n",
              dynsoa_view_len(view), frames);

  dynsoa_shutdown();
  return 0;
}
