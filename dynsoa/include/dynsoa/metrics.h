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

#pragma once
#include "types.h"
#include <cstdint>

namespace dynsoa {

struct Sample {
  const char* kernel;
  ViewId view;
  float warp_eff = 1.f;
  float branch_div = 0.f;
  float mem_coalesce = 1.f;
  float l2_miss_rate = 0.f;
  std::uint32_t time_us = 0;
  std::uint32_t p95_tile_us = 0;
  std::uint32_t p99_tile_us = 0;
};

void metrics_enable_csv(const char* path);
void emit_metric(const Sample& s);

FrameAgg aggregate(ViewId v, int window_frames);
void     metrics_note_frame_end(ViewId v, const Sample& s);

} // namespace dynsoa
