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

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>

#include "dynsoa/dynsoa.h"

using namespace dynsoa;

struct Stats {
  double mean_us=0, p95_us=0, p99_us=0, tail_ratio=0;
};

// ---------------- Kernels ----------------

static void k_physics(ViewId v, const KernelCtx& ctx) {
  int n = (int)view_len(v);
  float* px = (float*)column(v, "Position.x");
  float* vx = (float*)column(v, "Velocity.vx");
  if (!px || !vx) return;
  volatile float guard = 0.f;
  for (int i=0;i<n;++i) {
    float val = px[i] + vx[i]*ctx.dt;
    px[i] = val;
    guard += val * 1e-9f;
  }
  if (guard < -1e30f) std::cerr << ""; // keep compiler honest
}

static void k_branchy(ViewId v, const KernelCtx& ctx) {
  int n = (int)view_len(v);
  float* px = (float*)column(v, "Position.x");
  float* vx = (float*)column(v, "Velocity.vx");
  if (!px || !vx) return;
  for (int i=0;i<n;++i) {
    float x = px[i];
    if (x >  1000.0f) px[i] = x * 0.97f;
    else if (x < -1000.0f) px[i] = x * 1.03f;
    else px[i] = x + vx[i]*0.001f;
  }
}

static void k_scatter(ViewId v, const KernelCtx& ctx) {
  int n = (int)view_len(v);
  float* px = (float*)column(v, "Position.x");
  float* vx = (float*)column(v, "Velocity.vx");
  if (!px || !vx || n<=0) return;
  const int stride = 13;
  for (int i=0;i<n;++i) {
    int j = (i * stride) % n;
    px[j] += 0.5f * vx[i];
  }
}

static void k_block(ViewId v, const KernelCtx& ctx) {
  const char* comps[] = {"Position.x","Velocity.vx"};
  MatrixBlock mb = acquire_matrix_block(v, comps, 2, /*block_rows*/ 2048, /*offset_rows*/ 0);
  if (!mb.data || mb.rows <= 0 || mb.cols < 2) return;
  float* P = mb.data + 0*mb.leading_dim;
  float* V = mb.data + 1*mb.leading_dim;
  for (int r=0; r<mb.rows; ++r) {
    P[r] = P[r] + 0.25f * V[r];
  }
  release_matrix_block(v, &mb, /*write_back=*/true);
}

// ---------------- Helpers ----------------

static void init_entities(ViewId v) {
  int n = (int)view_len(v);
  float* px = (float*)column(v, "Position.x");
  float* vx = (float*)column(v, "Velocity.vx");
  if (!px || !vx) {
    std::cerr << "[init] null columns (px=" << (void*)px << " vx=" << (void*)vx << ")\n";
    return;
  }
  for (int i = 0; i < n; ++i) {
    px[i] = (float)i * 0.001f;
    vx[i] = 1.0f + (float)((i % 7) - 3) * 0.05f;
  }
}

struct MixStep {
  enum Kind { Physics, Branchy, Scatter, Block } kind;
  int period; // 1 = every frame; N = every N frames (used for Block)
};

static std::vector<MixStep> parse_mix(const std::string& mix) {
  // Example: "physics,branchy,scatter,block/8"
  std::vector<MixStep> out;
  std::stringstream ss(mix);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    if (tok == "physics") out.push_back({MixStep::Physics, 1});
    else if (tok == "branchy") out.push_back({MixStep::Branchy, 1});
    else if (tok == "scatter") out.push_back({MixStep::Scatter, 1});
    else if (tok.rfind("block",0)==0) {
      int period = 1;
      auto slash = tok.find('/');
      if (slash != std::string::npos) {
        period = std::max(1, std::atoi(tok.c_str()+slash+1));
      }
      out.push_back({MixStep::Block, period});
    }
  }
  if (out.empty()) { // sensible default
    out.push_back({MixStep::Physics, 1});
    out.push_back({MixStep::Branchy, 1});
    out.push_back({MixStep::Scatter, 1});
    out.push_back({MixStep::Block, 8});
  }
  return out;
}

static void run_mix_for_frame(ViewId v, const KernelCtx& ctx, const std::vector<MixStep>& mix, int64_t frame_index) {
  for (const auto& m : mix) {
    switch (m.kind) {
      case MixStep::Physics: dynsoa_run_kernel("k_physics", k_physics, v, &ctx); break;
      case MixStep::Branchy: dynsoa_run_kernel("k_branchy", k_branchy, v, &ctx); break;
      case MixStep::Scatter: dynsoa_run_kernel("k_scatter", k_scatter, v, &ctx); break;
      case MixStep::Block:
        if ((m.period <= 1) || (frame_index % m.period == 0)) {
          dynsoa_run_kernel("k_block", k_block, v, &ctx);
        }
        break;
    }
  }
}

struct RunConfig {
  int64_t entities = 1000000;
  int64_t frames   = 1000;
  int      budget  = 500; // us
  float    dt      = 0.016f;
  std::string mix  = "physics,branchy,scatter,block/8";
  std::string csv_path;
};

static Stats run_benchmark(ViewId v, const RunConfig& rc) {
  KernelCtx ctx{rc.dt, 0};
  std::vector<double> times; times.reserve((size_t)rc.frames);

  // Warmup
  dynsoa_begin_frame();
  run_mix_for_frame(v, ctx, parse_mix(rc.mix), /*frame_index*/0);
  dynsoa_end_frame();

  auto mix = parse_mix(rc.mix);
  for (int64_t f = 0; f < rc.frames; ++f) {
    auto t0 = std::chrono::high_resolution_clock::now();
    dynsoa_begin_frame();
    run_mix_for_frame(v, ctx, mix, f);
    dynsoa_end_frame();
    auto t1 = std::chrono::high_resolution_clock::now();
    double us = (double)std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    times.push_back(us);
  }

  std::sort(times.begin(), times.end());
  Stats S;
  if (!times.empty()) {
    double sum=0; for (double x: times) sum += x;
    S.mean_us = sum / times.size();
    auto idx95 = (size_t)std::min((size_t)(0.95 * (times.size()-1)), times.size()-1);
    auto idx99 = (size_t)std::min((size_t)(0.99 * (times.size()-1)), times.size()-1);
    S.p95_us = times[idx95];
    S.p99_us = times[idx99];
    S.tail_ratio = (S.p95_us>0) ? (S.p99_us / S.p95_us) : 0.0;
  }

  // Optional CSV summary (one line)
  if (!rc.csv_path.empty()) {
    std::ofstream fout(rc.csv_path, std::ios::app);
    if (fout) {
      fout << "entities,frames,dt_ms,budget_us,mix,mean_ms,p95_ms,p99_ms,tail\n";
      // overwrite header? append regardless for simplicity
    }
    fout.close();
    std::ofstream fout2(rc.csv_path, std::ios::app);
    if (fout2) {
      fout2
        << rc.entities << ","
        << rc.frames << ","
        << (rc.dt*1000.0f) << ","
        << rc.budget << ","
        << rc.mix << ","
        << (S.mean_us/1000.0) << ","
        << (S.p95_us/1000.0) << ","
        << (S.p99_us/1000.0) << ","
        << (S.tail_ratio) << "\n";
    }
  }

  return S;
}

static void printStats(const std::string& label, const Stats& A){
  std::cout.setf(std::ios::fixed); std::cout.precision(3);
  std::cout
    << label << " | mean = " << (A.mean_us/1000.0)
    << " ms | p95 = " << (A.p95_us/1000.0)
    << " ms | p99 = " << (A.p99_us/1000.0)
    << " ms | tail(p99/p95)= " << (A.p95_us>0?A.p99_us/A.p95_us:0.0)
    << "\n";
}

int main(int argc, char** argv) {
  RunConfig rc;

  // Parse args
  for (int i=1; i<argc; ++i) {
    if (!std::strcmp(argv[i], "--frames") && i+1<argc)   rc.frames   = std::atoll(argv[++i]);
    else if (!std::strcmp(argv[i], "--entities") && i+1<argc) rc.entities = std::atoll(argv[++i]);
    else if (!std::strcmp(argv[i], "--budget_us") && i+1<argc) rc.budget  = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--dt") && i+1<argc)        rc.dt      = std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--mix") && i+1<argc)       rc.mix     = argv[++i];
    else if (!std::strcmp(argv[i], "--csv") && i+1<argc)       rc.csv_path= argv[++i];
  }

  // Init config
  Config cfg;
  cfg.device = Device::CPU;
  cfg.aosoa_tile = 128;
  cfg.matrix_block = 1024;
  cfg.max_retile_us = rc.budget;
  cfg.scheduler_enabled = true;
  dynsoa_init(&cfg);

  // Define components & archetype
  Field posFields[] = { {"x", ScalarType::F32} };
  Field velFields[] = { {"vx", ScalarType::F32} };
  Component Position{ "Position", posFields, 1 };
  Component Velocity{ "Velocity", velFields, 1 };
  define_component(Position);
  define_component(Velocity);
  const char* comps[] = {"Position","Velocity"};
  ArchetypeId arch = define_archetype("Particle", comps, 2);

  // Storage + view
  spawn(arch, (std::size_t)rc.entities, nullptr);
  ViewId v = make_view(arch);
  if ((long long)view_len(v) <= 0) {
    v = make_view(arch);
  }
  init_entities(v);

  // Baseline: force SoA, disable adaptive
  retile_to_soa(v);
  {
    Policy P; P.cooloff_frames = 1000000; // effectively disable actions
    scheduler_set_policy(P);
  }
  Stats baseline = run_benchmark(v, rc);

  // Adaptive: enable AoSoA
  {
    Policy P;
    P.triggers.push_back({"mean_us >= 0", "RETILE_AOSOA", 128, 1.0});
    P.cooloff_frames = 5;
    scheduler_set_policy(P);
  }
  Stats adapt = run_benchmark(v, rc);

  std::cout << "\n=== DynSoA Mixed-Batch Benchmark ===\n";
  std::cout.setf(std::ios::fixed); std::cout.precision(3);
  std::cout << "entities=" << rc.entities
            << " frames="   << rc.frames
            << " dt="       << (rc.dt*1000.0f) << "ms"
            << " budget="   << rc.budget << "us"
            << " mix="      << rc.mix << "\n";

  printStats("SoA-fixed   ", baseline);
  printStats("Adaptive    ", adapt);

  auto safe_div = [](double num, double den){ return den>0? num/den : 0.0; };
  double mean_delta = safe_div( (baseline.mean_us - adapt.mean_us), baseline.mean_us);
  double p95_delta  = safe_div( (baseline.p95_us  - adapt.p95_us ), baseline.p95_us );
  double p99_delta  = safe_div( (baseline.p99_us  - adapt.p99_us ), baseline.p99_us );

  std::cout.setf(std::ios::fixed); std::cout.precision(1);
  std::cout << "Improvements: mean=" << (100.0*mean_delta)
            << "%  p95=" << (100.0*p95_delta)
            << "%  p99=" << (100.0*p99_delta) << "%\n";

  dynsoa_shutdown();
  return 0;
}