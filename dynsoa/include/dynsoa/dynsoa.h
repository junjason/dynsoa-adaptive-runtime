// DynSoA Runtime SDK


#pragma once
#include "dynsoa_export.h"
#include "types.h"
#include "schema.h"
#include "entity_store.h"
#include "layout.h"
#include "metrics.h"
#include "scheduler.h"
#include "kernels.h"

extern "C" {

DYNSOA_API void dynsoa_init(const dynsoa::Config* cfg);
DYNSOA_API void dynsoa_shutdown();

DYNSOA_API void dynsoa_define_component(const dynsoa::Component* c);
DYNSOA_API dynsoa::ArchetypeId dynsoa_define_archetype(const char* name, const char** comps, int count);

DYNSOA_API void*  dynsoa_spawn(dynsoa::ArchetypeId arch, size_t count, void(*init_fn)(size_t, void*));
DYNSOA_API dynsoa::ViewId dynsoa_make_view(dynsoa::ArchetypeId arch);
DYNSOA_API size_t dynsoa_view_len(dynsoa::ViewId v);
DYNSOA_API void*  dynsoa_column(dynsoa::ViewId v, const char* path);

// Retile helpers
DYNSOA_API int  dynsoa_retile_aosoa_plan_apply(dynsoa::ViewId v, int tile);
DYNSOA_API int  dynsoa_retile_to_soa(dynsoa::ViewId v);

// Matrix blocks
DYNSOA_API void* dynsoa_acquire_matrix_block(dynsoa::ViewId v, const char** comps, int k, int rows, size_t offset, dynsoa::MatrixBlock* out);
DYNSOA_API void  dynsoa_release_matrix_block(dynsoa::ViewId v, dynsoa::MatrixBlock* mb, int write_back);

// Scheduler/frames
DYNSOA_API void dynsoa_begin_frame();
DYNSOA_API void dynsoa_run_kernel(const char* name,
                                  void (*fn)(dynsoa::ViewId, const dynsoa::KernelCtx&),
                                  dynsoa::ViewId v,
                                  const dynsoa::KernelCtx* ctx);
DYNSOA_API void dynsoa_end_frame();
DYNSOA_API void dynsoa_set_policy(const char* json_or_empty);

// Metrics
DYNSOA_API void dynsoa_metrics_enable_csv(const char* path);
DYNSOA_API void dynsoa_emit_metric(const dynsoa::Sample* s);

}
