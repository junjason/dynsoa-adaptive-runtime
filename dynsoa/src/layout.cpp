// DynSoA Runtime SDK


#include "dynsoa/layout.h"
#include "dynsoa/entity_store.h"
#include "dynsoa/metrics.h"
#include "dynsoa/scheduler.h"
#include <algorithm>

namespace dynsoa {

static double mem_bw_bytes_per_us() { return 4096.0; } // heuristic

LayoutKind current_layout(ViewId v) {
  return entity_current_layout(v);
}

static std::size_t bytes_to_move_bridge(ViewId v) {
  return bytes_to_move(v);
}

static void soa_to_aosoa(ViewId v, int T) {
  transform_soa_to_aosoa(v, T);
}

static void aosoa_to_soa(ViewId v) {
  transform_aosoa_to_soa(v);
}

RetilePlan plan_aosoa(ViewId v, int tile) {
  RetilePlan p; p.to = LayoutKind::AoSoA; p.tile_or_block = tile;
  const double bytes = (double)bytes_to_move_bridge(v);
  p.est_cost_us = bytes / mem_bw_bytes_per_us();

  FrameAgg a = aggregate(v, 3);
  LearnState L = scheduler_learn_for();

  double div_term  = std::max(0.0, a.branch_div - 0.15);
  double mem_term  = std::max(0.0, 0.75 - a.mem_coalesce);
  double tail_term = std::max(0.0, a.tail_ratio - 1.10);
  double base      = (a.p95_us>0 ? a.p95_us : (a.mean_us>0 ? a.mean_us : 500.0));

  p.est_gain_us = base * (L.a_div*div_term + L.a_mem*mem_term + L.a_tail*tail_term);
  p.est_gain_us = std::max(30.0, std::min(p.est_gain_us, base * 0.35));
  return p;
}

RetilePlan plan_matrix(ViewId v, int block) {
  RetilePlan p; p.to = LayoutKind::Matrix; p.tile_or_block = block;
  const double bytes = (double)bytes_to_move_bridge(v);
  p.est_cost_us = 0.25 * (bytes / mem_bw_bytes_per_us());

  FrameAgg a = aggregate(v, 3);
  LearnState L = scheduler_learn_for();
  double mem_term  = std::max(0.0, 0.80 - a.mem_coalesce);
  double base      = (a.mean_us>0 ? a.mean_us : 400.0);

  p.est_gain_us = base * ( (0.8*L.a_mem) * mem_term );
  p.est_gain_us = std::max(15.0, std::min(p.est_gain_us, base * 0.20));
  return p;
}

bool retile_to_soa(ViewId v) { aosoa_to_soa(v); return true; }

bool retile(ViewId v, const RetilePlan& plan) {
  switch (plan.to) {
    case LayoutKind::AoSoA: soa_to_aosoa(v, plan.tile_or_block); return true;
    case LayoutKind::SoA:   aosoa_to_soa(v); return true;
    case LayoutKind::Matrix: return true; // transient via acquire_matrix_block
    case LayoutKind::AoS:
    default: break;
  }
  return false;
}

} // namespace dynsoa
