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
#include <mutex>

namespace {
  dynsoa::Config g_cfg;
  std::once_flag g_once;
  bool g_inited = false;
}

extern "C" {

// ---------------------------------------------------
// Lifecycle
// ---------------------------------------------------
void dynsoa_init(const dynsoa::Config* cfg) {
  std::call_once(g_once, [&]{
    if (cfg) g_cfg = *cfg;
    dynsoa::scheduler_load_state(); // load learned weights
    g_inited = true;
  });
}

void dynsoa_shutdown() {
  if (g_inited) {
    dynsoa::scheduler_save_state(); // persist learned weights
    g_inited = false;
  }
}

// ---------------------------------------------------
// Schema / storage
// ---------------------------------------------------
void dynsoa_define_component(const dynsoa::Component* c) {
  dynsoa::define_component(*c);
}

dynsoa::ArchetypeId dynsoa_define_archetype(const char* n, const char** comps, int k) {
  return dynsoa::define_archetype(n, comps, k);
}

void* dynsoa_spawn(dynsoa::ArchetypeId a, size_t n, void(*init_fn)(size_t,void*)) {
  return dynsoa::spawn(a, n, init_fn);
}

dynsoa::ViewId dynsoa_make_view(dynsoa::ArchetypeId a) { return dynsoa::make_view(a); }
size_t         dynsoa_view_len(dynsoa::ViewId v)       { return dynsoa::view_len(v); }
void*          dynsoa_column(dynsoa::ViewId v, const char* p) { return dynsoa::column(v, p); }

// ---------------------------------------------------
// Retile helpers / matrix blocks
// ---------------------------------------------------
int dynsoa_retile_aosoa_plan_apply(dynsoa::ViewId v, int tile) {
  return dynsoa::retile(v, dynsoa::plan_aosoa(v, tile)) ? 1 : 0;
}
int dynsoa_retile_to_soa(dynsoa::ViewId v) {
  return dynsoa::retile_to_soa(v) ? 1 : 0;
}

void* dynsoa_acquire_matrix_block(dynsoa::ViewId v, const char** comps, int k, int rows,
                                  size_t offset, dynsoa::MatrixBlock* out) {
  auto mb = dynsoa::acquire_matrix_block(v, comps, k, rows, offset);
  if (out) *out = mb;
  return (void*)mb.data;
}
void dynsoa_release_matrix_block(dynsoa::ViewId v, dynsoa::MatrixBlock* mb, int write_back) {
  dynsoa::release_matrix_block(v, mb, write_back != 0);
}

// ---------------------------------------------------
// Frame / scheduler
// ---------------------------------------------------
void dynsoa_begin_frame() {
  dynsoa::begin_frame();
  dynsoa::scheduler_on_begin_frame();
}

void dynsoa_run_kernel(const char* name,
                       void (*fn)(dynsoa::ViewId, const dynsoa::KernelCtx&),
                       dynsoa::ViewId v,
                       const dynsoa::KernelCtx* ctx) {
  dynsoa::run_kernel(name, fn, v, *ctx);
}

void dynsoa_end_frame() {
  dynsoa::scheduler_on_end_frame();
  dynsoa::end_frame();
}

// ---------------------------------------------------
// Policy (always-trigger for demo visibility)
// ---------------------------------------------------
void dynsoa_set_policy(const char* /*json_or_empty*/) {
  dynsoa::Policy P;
  // Simple "always true" trigger so we can observe actions & learning.
  P.triggers.push_back({"mean_us >= 0", "RETILE_AOSOA", 128, 1.0});
  P.cooloff_frames = 2;
  dynsoa::scheduler_set_policy(P);
}

// ---------------------------------------------------
// Metrics
// ---------------------------------------------------
void dynsoa_metrics_enable_csv(const char* path) { dynsoa::metrics_enable_csv(path); }
void dynsoa_emit_metric(const dynsoa::Sample* s) { dynsoa::emit_metric(*s); }

} // extern "C"
