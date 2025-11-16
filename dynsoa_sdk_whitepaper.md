# Dynamic SoA Runtime SDK — Technical Whitepaper

**Author:** Sungmin (Jason) Jun  
**Date:** November 04, 2025  
**Confidential / Proprietary — Not for Distribution**

---

## Executive Overview


This whitepaper documents the architecture and APIs of the Dynamic Structure-of-Arrays (Dynamic SoA) Runtime SDK.
The SDK implements a self-optimizing runtime that monitors execution metrics (e.g., memory coalescing, branch divergence,
tail latencies) and dynamically transforms data layouts (SoA ↔ AoSoA ↔ Matrix packing) to reduce latency on heterogeneous
parallel processors (CPUs/GPUs). A learning loop—augmented with a UCB1 bandit—selects and refines layout choices online.

**Value for non-engineers:** The SDK automatically discovers faster memory layouts during execution. This yields measurable
latency reductions without requiring developers to rewrite application logic—improving frame rates and throughput while
cutting compute costs in simulations, games, and analytics pipelines.

## System Architecture

![Architecture Diagram](dynsoa_architecture.png)


**Data Path:** Application kernels access columns from the Entity Store. The Layout Engine retile plans (SoA/AoSoA/Matrix) change
how columns are arranged to improve locality. Metrics capture per-kernel timing and memory signals; the Scheduler’s policy and
bandit choose layout actions; the Learning loop updates coefficients based on realized improvements, persisting them for future runs.

## Modules and Responsibilities

### `include`

Public headers that define the SDK types and external API surface.

**Files:**

- `include/dynsoa/dynsoa.h`
- `include/dynsoa/dynsoa_export.h`
- `include/dynsoa/entity_store.h`
- `include/dynsoa/kernels.h`
- `include/dynsoa/layout.h`
- `include/dynsoa/metrics.h`
- `include/dynsoa/scheduler.h`
- `include/dynsoa/schema.h`
- `include/dynsoa/types.h`

**Key Functions (signatures):**

- `DYNSOA_API void dynsoa_init(const dynsoa::Config* cfg)`
  - Initialize and/or load scheduler state; prepare runtime.
- `DYNSOA_API void dynsoa_shutdown()`
  - Shutdown and persist state; cleanup resources.
- `DYNSOA_API void dynsoa_define_component(const dynsoa::Component* c)`
  - Register a component type with the entity system.
- `DYNSOA_API dynsoa::ArchetypeId dynsoa_define_archetype(const char* name, const char** comps, int count)`
  - Declare an archetype (set of components) and return its ID.
- `DYNSOA_API dynsoa::ViewId dynsoa_make_view(dynsoa::ArchetypeId arch)`
  - Create a logical view over an archetype’s storage.
- `DYNSOA_API size_t dynsoa_view_len(dynsoa::ViewId v)`
  - Return number of rows/entities in a view.
- `DYNSOA_API void* dynsoa_column(dynsoa::ViewId v, const char* path)`
  - Return a pointer/handle to a component column.
- `Retile helpers
DYNSOA_API int dynsoa_retile_aosoa_plan_apply(dynsoa::ViewId v, int tile)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `DYNSOA_API int dynsoa_retile_to_soa(dynsoa::ViewId v)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `Matrix blocks
DYNSOA_API void* dynsoa_acquire_matrix_block(dynsoa::ViewId v, const char** comps, int k, int rows, size_t offset, dynsoa::MatrixBlock* out)`
  - Acquire a temporary matrix-packed view/region.
- `DYNSOA_API void dynsoa_release_matrix_block(dynsoa::ViewId v, dynsoa::MatrixBlock* mb, int write_back)`
  - Release a matrix block; optional write-back.
- `frames
DYNSOA_API void dynsoa_begin_frame()`
  - Start frame; reset metrics windows; begin scheduling epoch.
- `DYNSOA_API void dynsoa_end_frame()`
  - End frame; trigger scheduling decisions and learning updates.
- `DYNSOA_API void dynsoa_set_policy(const char* json_or_empty)`
  - See source comments for details; part of SDK public or internal API.
- `Metrics
DYNSOA_API void dynsoa_metrics_enable_csv(const char* path)`
  - Metrics/telemetry hook.
- `DYNSOA_API void dynsoa_emit_metric(const dynsoa::Sample* s)`
  - See source comments for details; part of SDK public or internal API.
- `ViewId make_view(ArchetypeId arch)`
  - Create a logical view over an archetype’s storage.
- `size_t view_len(ViewId v)`
  - Return number of rows/entities in a view.
- `void* column(ViewId v, const char* path)`
  - Return a pointer/handle to a component column.
- `MatrixBlock acquire_matrix_block(ViewId v, const char** comps, int k, int block_rows, std::size_t offset_rows=0)`
  - Acquire a temporary matrix-packed view/region.
- `void release_matrix_block(ViewId v, MatrixBlock* mb, bool write_back)`
  - Release a matrix block; optional write-back.
- `Extra helpers to avoid exposing internal ViewRec to other translation units
std::size_t bytes_to_move(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `LayoutKind entity_current_layout(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `void transform_soa_to_aosoa(ViewId v, int tile)`
  - See source comments for details; part of SDK public or internal API.
- `void transform_aosoa_to_soa(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `void begin_frame()`
  - Start frame; reset metrics windows; begin scheduling epoch.
- `void run_kernel(const char* name, KernelFn fn, ViewId v, const KernelCtx& ctx)`
  - Run a named kernel over a view, capturing metrics.
- `void end_frame()`
  - End frame; trigger scheduling decisions and learning updates.
- `LayoutKind current_layout(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `RetilePlan plan_aosoa(ViewId v, int tile)`
  - Construct an AoSoA retile plan for a given tile size.
- `RetilePlan plan_matrix(ViewId v, int block)`
  - Construct a Matrix pack plan for a given block size.
- `bool retile(ViewId v, const RetilePlan& plan)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `bool retile_to_soa(ViewId v)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `void metrics_enable_csv(const char* path)`
  - Metrics/telemetry hook.
- `void emit_metric(const Sample& s)`
  - See source comments for details; part of SDK public or internal API.
- `FrameAgg aggregate(ViewId v, int window_frames)`
  - See source comments for details; part of SDK public or internal API.
- `void metrics_note_frame_end(ViewId v, const Sample& s)`
  - Metrics/telemetry hook.
- `void scheduler_set_policy(const Policy& p)`
  - Scheduler control/learning helper.
- `void scheduler_on_begin_frame()`
  - Start frame; reset metrics windows; begin scheduling epoch.
- `void scheduler_on_end_frame()`
  - End frame; trigger scheduling decisions and learning updates.
- `Learning & persistence
LearnState scheduler_learn_for()`
  - Scheduler control/learning helper.
- `void scheduler_set_persist_path(const char*)`
  - Scheduler control/learning helper.
- `void scheduler_load_state()`
  - Scheduler control/learning helper.
- `void scheduler_save_state()`
  - Scheduler control/learning helper.
- `void define_component(const Component& c)`
  - Register a component type with the entity system.
- `ArchetypeId define_archetype(const char* name, const char** components, int count)`
  - Declare an archetype (set of components) and return its ID.

### `src`

Core implementation of entity storage, layout transforms, scheduler, and metrics.

**Files:**

- `src/dynsoa.cpp`
- `src/entity_store.cpp`
- `src/kernels.cpp`
- `src/layout.cpp`
- `src/metrics.cpp`
- `src/scheduler.cpp`
- `src/schema.cpp`

**Key Functions (signatures):**

- `void dynsoa_init(const dynsoa::Config* cfg)`
  - Initialize and/or load scheduler state; prepare runtime.
- `void dynsoa_shutdown()`
  - Shutdown and persist state; cleanup resources.
- `void dynsoa_define_component(const dynsoa::Component* c)`
  - Register a component type with the entity system.
- `dynsoa::ArchetypeId dynsoa_define_archetype(const char* n, const char** comps, int k)`
  - Declare an archetype (set of components) and return its ID.
- `return dynsoa::define_archetype(n, comps, k)`
  - Declare an archetype (set of components) and return its ID.
- `return dynsoa::spawn(a, n, init_fn)`
  - Create entities for an archetype; optionally run an initializer.
- `dynsoa::ViewId dynsoa_make_view(dynsoa::ArchetypeId a)`
  - Create a logical view over an archetype’s storage.
- `return dynsoa::make_view(a)`
  - Create a logical view over an archetype’s storage.
- `size_t dynsoa_view_len(dynsoa::ViewId v)`
  - Return number of rows/entities in a view.
- `return dynsoa::view_len(v)`
  - Return number of rows/entities in a view.
- `void* dynsoa_column(dynsoa::ViewId v, const char* p)`
  - Return a pointer/handle to a component column.
- `return dynsoa::column(v, p)`
  - Return a pointer/handle to a component column.
- `int dynsoa_retile_aosoa_plan_apply(dynsoa::ViewId v, int tile)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `int dynsoa_retile_to_soa(dynsoa::ViewId v)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `void* dynsoa_acquire_matrix_block(dynsoa::ViewId v, const char** comps, int k, int rows,
                                  size_t offset, dynsoa::MatrixBlock* out)`
  - Acquire a temporary matrix-packed view/region.
- `void dynsoa_release_matrix_block(dynsoa::ViewId v, dynsoa::MatrixBlock* mb, int write_back)`
  - Release a matrix block; optional write-back.
- `void dynsoa_begin_frame()`
  - Start frame; reset metrics windows; begin scheduling epoch.
- `void dynsoa_end_frame()`
  - End frame; trigger scheduling decisions and learning updates.
- `void dynsoa_set_policy(const char* /*json_or_empty*/)`
  - See source comments for details; part of SDK public or internal API.
- `void dynsoa_metrics_enable_csv(const char* path)`
  - Metrics/telemetry hook.
- `void dynsoa_emit_metric(const dynsoa::Sample* s)`
  - See source comments for details; part of SDK public or internal API.
- `ViewId make_view(ArchetypeId arch)`
  - Create a logical view over an archetype’s storage.
- `size_t view_len(ViewId v)`
  - Return number of rows/entities in a view.
- `void* column(ViewId v, const char* path)`
  - Return a pointer/handle to a component column.
- `MatrixBlock acquire_matrix_block(ViewId v, const char** comps, int K, int B, std::size_t offset)`
  - Acquire a temporary matrix-packed view/region.
- `void release_matrix_block(ViewId v, MatrixBlock* mb, bool write_back)`
  - Release a matrix block; optional write-back.
- `std::size_t bytes_to_move(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `LayoutKind entity_current_layout(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `void transform_soa_to_aosoa(ViewId v, int T)`
  - See source comments for details; part of SDK public or internal API.
- `void transform_aosoa_to_soa(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `void begin_frame()`
  - Start frame; reset metrics windows; begin scheduling epoch.
- `void run_kernel(const char* name, KernelFn fn, ViewId v, const KernelCtx& ctx)`
  - Run a named kernel over a view, capturing metrics.
- `void end_frame()`
  - End frame; trigger scheduling decisions and learning updates.
- `static double mem_bw_bytes_per_us()`
  - See source comments for details; part of SDK public or internal API.
- `heuristic

LayoutKind current_layout(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `return entity_current_layout(v)`
  - See source comments for details; part of SDK public or internal API.
- `static std::size_t bytes_to_move_bridge(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `return bytes_to_move(v)`
  - See source comments for details; part of SDK public or internal API.
- `static void soa_to_aosoa(ViewId v, int T)`
  - See source comments for details; part of SDK public or internal API.
- `static void aosoa_to_soa(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `RetilePlan plan_aosoa(ViewId v, int tile)`
  - Construct an AoSoA retile plan for a given tile size.
- `RetilePlan plan_matrix(ViewId v, int block)`
  - Construct a Matrix pack plan for a given block size.
- `bool retile_to_soa(ViewId v)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `bool retile(ViewId v, const RetilePlan& plan)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `case LayoutKind::AoSoA: soa_to_aosoa(v, plan.tile_or_block)`
  - See source comments for details; part of SDK public or internal API.
- `case LayoutKind::SoA: aosoa_to_soa(v)`
  - See source comments for details; part of SDK public or internal API.
- `void metrics_enable_csv(const char* path)`
  - Metrics/telemetry hook.
- `std::lock_guard<std::mutex> lk(g_mu)`
  - See source comments for details; part of SDK public or internal API.
- `void emit_metric(const Sample& s)`
  - See source comments for details; part of SDK public or internal API.
- `void metrics_note_frame_end(ViewId v, const Sample& s)`
  - Metrics/telemetry hook.
- `time_us : lerp(E.mean_us, s.time_us)`
  - See source comments for details; part of SDK public or internal API.
- `warp_eff: lerp(E.warp_eff, s.warp_eff)`
  - See source comments for details; part of SDK public or internal API.
- `p95_tile_us : lerp(E.p95_us, s.p95_tile_us)`
  - See source comments for details; part of SDK public or internal API.
- `p99_tile_us : lerp(E.p99_us, s.p99_tile_us)`
  - See source comments for details; part of SDK public or internal API.
- `FrameAgg aggregate(ViewId v, int window_frames)`
  - See source comments for details; part of SDK public or internal API.
- `static void ensure_verbose_init()`
  - Initialize and/or load scheduler state; prepare runtime.
- `static void vprint(const std::string& s)`
  - See source comments for details; part of SDK public or internal API.
- `void update(double r)`
  - See source comments for details; part of SDK public or internal API.
- `double var()`
  - See source comments for details; part of SDK public or internal API.
- `candidate actions we consider each decision epoch
static std::vector<RetilePlan> catalog_actions(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `static RetilePlan pick_with_ucb(ViewId v, const std::vector<RetilePlan>& C)`
  - See source comments for details; part of SDK public or internal API.
- `Update bandit stat after learning step
static void bandit_update(ViewId v, const RetilePlan& p, double realized_us)`
  - See source comments for details; part of SDK public or internal API.
- `static double field_value(const std::string& name, const FrameAgg& a)`
  - See source comments for details; part of SDK public or internal API.
- `static void trim(std::string& s)`
  - See source comments for details; part of SDK public or internal API.
- `static bool eval_atom(std::string expr, const FrameAgg& a)`
  - See source comments for details; part of SDK public or internal API.
- `static bool eval_pred(const std::string& when, const FrameAgg& a)`
  - See source comments for details; part of SDK public or internal API.
- `return eval_atom(when, a)`
  - See source comments for details; part of SDK public or internal API.
- `void scheduler_set_policy(const Policy& p)`
  - Scheduler control/learning helper.
- `void scheduler_on_begin_frame()`
  - Start frame; reset metrics windows; begin scheduling epoch.
- `void scheduler_on_end_frame()`
  - End frame; trigger scheduling decisions and learning updates.
- `else retile(c.v, c.plan)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `LearnState scheduler_learn_for()`
  - Scheduler control/learning helper.
- `void scheduler_set_persist_path(const char* p)`
  - Scheduler control/learning helper.
- `void scheduler_load_state()`
  - Scheduler control/learning helper.
- `std::ifstream fin(g_persist_path)`
  - See source comments for details; part of SDK public or internal API.
- `void scheduler_save_state()`
  - Scheduler control/learning helper.
- `std::ofstream fout(g_persist_path, std::ios::out | std::ios::trunc)`
  - See source comments for details; part of SDK public or internal API.
- `void define_component(const Component& c)`
  - Register a component type with the entity system.
- `ArchetypeId define_archetype(const char* name, const char** comps, int count)`
  - Declare an archetype (set of components) and return its ID.

### `tests`

Smoke/benchmark harness to exercise kernels and generate metrics.

**Files:**

- `tests/smoke_main.cpp`

**Key Functions (signatures):**

- `static void physics_kernel(ViewId v, const KernelCtx& ctx)`
  - See source comments for details; part of SDK public or internal API.
- `int main()`
  - See source comments for details; part of SDK public or internal API.

## Adaptive Learning & Scheduling


The scheduler decides when and how to retile using two layers:

1. **Policy Triggers** – declarative conditions (e.g., `mean_us >= 0`, `mem_coalesce < 0.7`) that gate action eligibility.  
2. **UCB1 Bandit** – chooses among candidate plans (e.g., `AoSoA{64,128,256}`, `Matrix{64}`) by maximizing an upper-confidence bound on
   net improvement, where each action’s **reward** is `realized_us - est_cost_us`. This balances exploration (trying new tiles) and exploitation
   (reusing tiles that worked).

After an action, the runtime observes the **baseline vs. post-action** latency over a short horizon. The learning loop updates signal weights
(e.g., `a_div`, `a_mem`, `a_tail`) so gain estimates become more accurate over time. The state persists to `dynsoa_learn.json`.

## Integration Guide


**Minimal workflow (C++):**
```cpp
dynsoa_init(&cfg);
dynsoa_define_component(&Position);
dynsoa_define_component(&Velocity);
ArchetypeId arch = dynsoa_define_archetype("Body", comps, 2);
void* storage = dynsoa_spawn(arch, N, init_fn);
ViewId view = dynsoa_make_view(arch);

for (int f=0; f<frames; ++f) {
  dynsoa_begin_frame();
  dynsoa_run_kernel("physics_step", physics_kernel, view, &ctx);
  dynsoa_end_frame();
}

dynsoa_shutdown();
```
**Unity/Unreal:** Wrap `dynsoa_*` calls behind engine-native components/systems and call per-frame. Use `DYNSOA_VERBOSE` and `DYNSOA_LEARN_LOG`
to record scheduler actions for tuning and demos.

## Appendix: Function Reference (Condensed)

### include

- `DYNSOA_API void dynsoa_init(const dynsoa::Config* cfg)`
  - Initialize and/or load scheduler state; prepare runtime.
- `DYNSOA_API void dynsoa_shutdown()`
  - Shutdown and persist state; cleanup resources.
- `DYNSOA_API void dynsoa_define_component(const dynsoa::Component* c)`
  - Register a component type with the entity system.
- `DYNSOA_API dynsoa::ArchetypeId dynsoa_define_archetype(const char* name, const char** comps, int count)`
  - Declare an archetype (set of components) and return its ID.
- `DYNSOA_API dynsoa::ViewId dynsoa_make_view(dynsoa::ArchetypeId arch)`
  - Create a logical view over an archetype’s storage.
- `DYNSOA_API size_t dynsoa_view_len(dynsoa::ViewId v)`
  - Return number of rows/entities in a view.
- `DYNSOA_API void* dynsoa_column(dynsoa::ViewId v, const char* path)`
  - Return a pointer/handle to a component column.
- `Retile helpers
DYNSOA_API int dynsoa_retile_aosoa_plan_apply(dynsoa::ViewId v, int tile)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `DYNSOA_API int dynsoa_retile_to_soa(dynsoa::ViewId v)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `Matrix blocks
DYNSOA_API void* dynsoa_acquire_matrix_block(dynsoa::ViewId v, const char** comps, int k, int rows, size_t offset, dynsoa::MatrixBlock* out)`
  - Acquire a temporary matrix-packed view/region.
- `DYNSOA_API void dynsoa_release_matrix_block(dynsoa::ViewId v, dynsoa::MatrixBlock* mb, int write_back)`
  - Release a matrix block; optional write-back.
- `frames
DYNSOA_API void dynsoa_begin_frame()`
  - Start frame; reset metrics windows; begin scheduling epoch.
- `DYNSOA_API void dynsoa_end_frame()`
  - End frame; trigger scheduling decisions and learning updates.
- `DYNSOA_API void dynsoa_set_policy(const char* json_or_empty)`
  - See source comments for details; part of SDK public or internal API.
- `Metrics
DYNSOA_API void dynsoa_metrics_enable_csv(const char* path)`
  - Metrics/telemetry hook.
- `DYNSOA_API void dynsoa_emit_metric(const dynsoa::Sample* s)`
  - See source comments for details; part of SDK public or internal API.
- `ViewId make_view(ArchetypeId arch)`
  - Create a logical view over an archetype’s storage.
- `size_t view_len(ViewId v)`
  - Return number of rows/entities in a view.
- `void* column(ViewId v, const char* path)`
  - Return a pointer/handle to a component column.
- `MatrixBlock acquire_matrix_block(ViewId v, const char** comps, int k, int block_rows, std::size_t offset_rows=0)`
  - Acquire a temporary matrix-packed view/region.
- `void release_matrix_block(ViewId v, MatrixBlock* mb, bool write_back)`
  - Release a matrix block; optional write-back.
- `Extra helpers to avoid exposing internal ViewRec to other translation units
std::size_t bytes_to_move(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `LayoutKind entity_current_layout(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `void transform_soa_to_aosoa(ViewId v, int tile)`
  - See source comments for details; part of SDK public or internal API.
- `void transform_aosoa_to_soa(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `void begin_frame()`
  - Start frame; reset metrics windows; begin scheduling epoch.
- `void run_kernel(const char* name, KernelFn fn, ViewId v, const KernelCtx& ctx)`
  - Run a named kernel over a view, capturing metrics.
- `void end_frame()`
  - End frame; trigger scheduling decisions and learning updates.
- `LayoutKind current_layout(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `RetilePlan plan_aosoa(ViewId v, int tile)`
  - Construct an AoSoA retile plan for a given tile size.
- `RetilePlan plan_matrix(ViewId v, int block)`
  - Construct a Matrix pack plan for a given block size.
- `bool retile(ViewId v, const RetilePlan& plan)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `bool retile_to_soa(ViewId v)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `void metrics_enable_csv(const char* path)`
  - Metrics/telemetry hook.
- `void emit_metric(const Sample& s)`
  - See source comments for details; part of SDK public or internal API.
- `FrameAgg aggregate(ViewId v, int window_frames)`
  - See source comments for details; part of SDK public or internal API.
- `void metrics_note_frame_end(ViewId v, const Sample& s)`
  - Metrics/telemetry hook.
- `void scheduler_set_policy(const Policy& p)`
  - Scheduler control/learning helper.
- `void scheduler_on_begin_frame()`
  - Start frame; reset metrics windows; begin scheduling epoch.
- `void scheduler_on_end_frame()`
  - End frame; trigger scheduling decisions and learning updates.
- `Learning & persistence
LearnState scheduler_learn_for()`
  - Scheduler control/learning helper.
- `void scheduler_set_persist_path(const char*)`
  - Scheduler control/learning helper.
- `void scheduler_load_state()`
  - Scheduler control/learning helper.
- `void scheduler_save_state()`
  - Scheduler control/learning helper.
- `void define_component(const Component& c)`
  - Register a component type with the entity system.
- `ArchetypeId define_archetype(const char* name, const char** components, int count)`
  - Declare an archetype (set of components) and return its ID.

### src

- `void dynsoa_init(const dynsoa::Config* cfg)`
  - Initialize and/or load scheduler state; prepare runtime.
- `void dynsoa_shutdown()`
  - Shutdown and persist state; cleanup resources.
- `void dynsoa_define_component(const dynsoa::Component* c)`
  - Register a component type with the entity system.
- `dynsoa::ArchetypeId dynsoa_define_archetype(const char* n, const char** comps, int k)`
  - Declare an archetype (set of components) and return its ID.
- `return dynsoa::define_archetype(n, comps, k)`
  - Declare an archetype (set of components) and return its ID.
- `return dynsoa::spawn(a, n, init_fn)`
  - Create entities for an archetype; optionally run an initializer.
- `dynsoa::ViewId dynsoa_make_view(dynsoa::ArchetypeId a)`
  - Create a logical view over an archetype’s storage.
- `return dynsoa::make_view(a)`
  - Create a logical view over an archetype’s storage.
- `size_t dynsoa_view_len(dynsoa::ViewId v)`
  - Return number of rows/entities in a view.
- `return dynsoa::view_len(v)`
  - Return number of rows/entities in a view.
- `void* dynsoa_column(dynsoa::ViewId v, const char* p)`
  - Return a pointer/handle to a component column.
- `return dynsoa::column(v, p)`
  - Return a pointer/handle to a component column.
- `int dynsoa_retile_aosoa_plan_apply(dynsoa::ViewId v, int tile)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `int dynsoa_retile_to_soa(dynsoa::ViewId v)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `void* dynsoa_acquire_matrix_block(dynsoa::ViewId v, const char** comps, int k, int rows,
                                  size_t offset, dynsoa::MatrixBlock* out)`
  - Acquire a temporary matrix-packed view/region.
- `void dynsoa_release_matrix_block(dynsoa::ViewId v, dynsoa::MatrixBlock* mb, int write_back)`
  - Release a matrix block; optional write-back.
- `void dynsoa_begin_frame()`
  - Start frame; reset metrics windows; begin scheduling epoch.
- `void dynsoa_end_frame()`
  - End frame; trigger scheduling decisions and learning updates.
- `void dynsoa_set_policy(const char* /*json_or_empty*/)`
  - See source comments for details; part of SDK public or internal API.
- `void dynsoa_metrics_enable_csv(const char* path)`
  - Metrics/telemetry hook.
- `void dynsoa_emit_metric(const dynsoa::Sample* s)`
  - See source comments for details; part of SDK public or internal API.
- `ViewId make_view(ArchetypeId arch)`
  - Create a logical view over an archetype’s storage.
- `size_t view_len(ViewId v)`
  - Return number of rows/entities in a view.
- `void* column(ViewId v, const char* path)`
  - Return a pointer/handle to a component column.
- `MatrixBlock acquire_matrix_block(ViewId v, const char** comps, int K, int B, std::size_t offset)`
  - Acquire a temporary matrix-packed view/region.
- `void release_matrix_block(ViewId v, MatrixBlock* mb, bool write_back)`
  - Release a matrix block; optional write-back.
- `std::size_t bytes_to_move(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `LayoutKind entity_current_layout(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `void transform_soa_to_aosoa(ViewId v, int T)`
  - See source comments for details; part of SDK public or internal API.
- `void transform_aosoa_to_soa(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `void begin_frame()`
  - Start frame; reset metrics windows; begin scheduling epoch.
- `void run_kernel(const char* name, KernelFn fn, ViewId v, const KernelCtx& ctx)`
  - Run a named kernel over a view, capturing metrics.
- `void end_frame()`
  - End frame; trigger scheduling decisions and learning updates.
- `static double mem_bw_bytes_per_us()`
  - See source comments for details; part of SDK public or internal API.
- `heuristic

LayoutKind current_layout(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `return entity_current_layout(v)`
  - See source comments for details; part of SDK public or internal API.
- `static std::size_t bytes_to_move_bridge(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `return bytes_to_move(v)`
  - See source comments for details; part of SDK public or internal API.
- `static void soa_to_aosoa(ViewId v, int T)`
  - See source comments for details; part of SDK public or internal API.
- `static void aosoa_to_soa(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `RetilePlan plan_aosoa(ViewId v, int tile)`
  - Construct an AoSoA retile plan for a given tile size.
- `RetilePlan plan_matrix(ViewId v, int block)`
  - Construct a Matrix pack plan for a given block size.
- `bool retile_to_soa(ViewId v)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `bool retile(ViewId v, const RetilePlan& plan)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `case LayoutKind::AoSoA: soa_to_aosoa(v, plan.tile_or_block)`
  - See source comments for details; part of SDK public or internal API.
- `case LayoutKind::SoA: aosoa_to_soa(v)`
  - See source comments for details; part of SDK public or internal API.
- `void metrics_enable_csv(const char* path)`
  - Metrics/telemetry hook.
- `std::lock_guard<std::mutex> lk(g_mu)`
  - See source comments for details; part of SDK public or internal API.
- `void emit_metric(const Sample& s)`
  - See source comments for details; part of SDK public or internal API.
- `void metrics_note_frame_end(ViewId v, const Sample& s)`
  - Metrics/telemetry hook.
- `time_us : lerp(E.mean_us, s.time_us)`
  - See source comments for details; part of SDK public or internal API.
- `warp_eff: lerp(E.warp_eff, s.warp_eff)`
  - See source comments for details; part of SDK public or internal API.
- `p95_tile_us : lerp(E.p95_us, s.p95_tile_us)`
  - See source comments for details; part of SDK public or internal API.
- `p99_tile_us : lerp(E.p99_us, s.p99_tile_us)`
  - See source comments for details; part of SDK public or internal API.
- `FrameAgg aggregate(ViewId v, int window_frames)`
  - See source comments for details; part of SDK public or internal API.
- `static void ensure_verbose_init()`
  - Initialize and/or load scheduler state; prepare runtime.
- `static void vprint(const std::string& s)`
  - See source comments for details; part of SDK public or internal API.
- `void update(double r)`
  - See source comments for details; part of SDK public or internal API.
- `double var()`
  - See source comments for details; part of SDK public or internal API.
- `candidate actions we consider each decision epoch
static std::vector<RetilePlan> catalog_actions(ViewId v)`
  - See source comments for details; part of SDK public or internal API.
- `static RetilePlan pick_with_ucb(ViewId v, const std::vector<RetilePlan>& C)`
  - See source comments for details; part of SDK public or internal API.
- `Update bandit stat after learning step
static void bandit_update(ViewId v, const RetilePlan& p, double realized_us)`
  - See source comments for details; part of SDK public or internal API.
- `static double field_value(const std::string& name, const FrameAgg& a)`
  - See source comments for details; part of SDK public or internal API.
- `static void trim(std::string& s)`
  - See source comments for details; part of SDK public or internal API.
- `static bool eval_atom(std::string expr, const FrameAgg& a)`
  - See source comments for details; part of SDK public or internal API.
- `static bool eval_pred(const std::string& when, const FrameAgg& a)`
  - See source comments for details; part of SDK public or internal API.
- `return eval_atom(when, a)`
  - See source comments for details; part of SDK public or internal API.
- `void scheduler_set_policy(const Policy& p)`
  - Scheduler control/learning helper.
- `void scheduler_on_begin_frame()`
  - Start frame; reset metrics windows; begin scheduling epoch.
- `void scheduler_on_end_frame()`
  - End frame; trigger scheduling decisions and learning updates.
- `else retile(c.v, c.plan)`
  - Apply a layout transformation (SoA/AoSoA/Matrix).
- `LearnState scheduler_learn_for()`
  - Scheduler control/learning helper.
- `void scheduler_set_persist_path(const char* p)`
  - Scheduler control/learning helper.
- `void scheduler_load_state()`
  - Scheduler control/learning helper.
- `std::ifstream fin(g_persist_path)`
  - See source comments for details; part of SDK public or internal API.
- `void scheduler_save_state()`
  - Scheduler control/learning helper.
- `std::ofstream fout(g_persist_path, std::ios::out | std::ios::trunc)`
  - See source comments for details; part of SDK public or internal API.
- `void define_component(const Component& c)`
  - Register a component type with the entity system.
- `ArchetypeId define_archetype(const char* name, const char** comps, int count)`
  - Declare an archetype (set of components) and return its ID.

### tests

- `static void physics_kernel(ViewId v, const KernelCtx& ctx)`
  - See source comments for details; part of SDK public or internal API.
- `int main()`
  - See source comments for details; part of SDK public or internal API.
