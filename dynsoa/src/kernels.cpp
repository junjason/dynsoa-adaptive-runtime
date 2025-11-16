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

#include "dynsoa/kernels.h"
#include "dynsoa/metrics.h"
#include <chrono>

namespace dynsoa {

void begin_frame() { /* scheduler prep via scheduler_on_begin_frame */ }

void run_kernel(const char* name, KernelFn fn, ViewId v, const KernelCtx& ctx) {
  auto t0 = std::chrono::high_resolution_clock::now();
  fn(v, ctx);
  auto t1 = std::chrono::high_resolution_clock::now();
  std::uint32_t us = (std::uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
  Sample s; s.kernel = name; s.view = v; s.time_us = us;
  emit_metric(s);
  metrics_note_frame_end(v, s);
}

void end_frame() { /* scheduler acts in scheduler_on_end_frame */ }

} // namespace dynsoa
