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

#include "dynsoa/metrics.h"
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <deque>
#include <cmath>

namespace dynsoa {

static std::mutex g_mu;
static std::ofstream g_csv;

struct AggState {
  std::deque<Sample> window;
  FrameAgg ewma;
};

static std::unordered_map<ViewId, AggState> g_agg;

void metrics_enable_csv(const char* path) {
  std::lock_guard<std::mutex> lk(g_mu);
  if (g_csv.is_open()) g_csv.close();
  g_csv.open(path, std::ios::out | std::ios::trunc);
  if (g_csv.is_open()) {
    g_csv << "kernel,view,time_us,p95_tile_us,p99_tile_us,warp_eff,branch_div,mem_coalesce,l2_miss_rate\n";
    g_csv.flush();
  }
}

void emit_metric(const Sample& s) {
  std::lock_guard<std::mutex> lk(g_mu);
  if (g_csv.is_open()) {
    g_csv << s.kernel << "," << s.view << "," << s.time_us << ","
          << s.p95_tile_us << "," << s.p99_tile_us << ","
          << s.warp_eff << "," << s.branch_div << ","
          << s.mem_coalesce << "," << s.l2_miss_rate << "\n";
  }
  g_agg[s.view].window.push_back(s);
  if (g_agg[s.view].window.size() > 120) g_agg[s.view].window.pop_front();
}

void metrics_note_frame_end(ViewId v, const Sample& s) {
  auto& E = g_agg[v].ewma;
  const double a = 0.2;
  auto lerp = [&](double cur, double obs){ return (1-a)*cur + a*obs; };
  E.mean_us      = (E.mean_us==0) ? s.time_us : lerp(E.mean_us, s.time_us);
  E.warp_eff     = (E.warp_eff==0)? s.warp_eff: lerp(E.warp_eff, s.warp_eff);
  E.branch_div   = lerp(E.branch_div, s.branch_div);
  E.mem_coalesce = lerp(E.mem_coalesce, s.mem_coalesce);
  E.l2_miss      = lerp(E.l2_miss, s.l2_miss_rate);
  E.p95_us       = (E.p95_us==0)? s.p95_tile_us : lerp(E.p95_us, s.p95_tile_us);
  E.p99_us       = (E.p99_us==0)? s.p99_tile_us : lerp(E.p99_us, s.p99_tile_us);
  E.tail_ratio   = (E.p95_us>0) ? (E.p99_us / E.p95_us) : 0.0;
}

FrameAgg aggregate(ViewId v, int window_frames) {
  FrameAgg A{};
  auto it = g_agg.find(v);
  if (it == g_agg.end()) return A;
  auto& dq = it->second.window;
  int n = 0;
  for (int i=(int)dq.size()-1; i>=0 && n<window_frames; --i, ++n) {
    A.mean_us      += dq[i].time_us;
    A.warp_eff     += dq[i].warp_eff;
    A.branch_div   += dq[i].branch_div;
    A.mem_coalesce += dq[i].mem_coalesce;
    A.l2_miss      += dq[i].l2_miss_rate;
    A.p95_us        = dq[i].p95_tile_us;
    A.p99_us        = dq[i].p99_tile_us;
  }
  if (n>0) {
    A.mean_us      /= n;
    A.warp_eff     /= n;
    A.branch_div   /= n;
    A.mem_coalesce /= n;
    A.l2_miss      /= n;
    A.tail_ratio = (A.p95_us>0) ? (A.p99_us/A.p95_us) : 0;
  }
  return A;
}

} // namespace dynsoa
