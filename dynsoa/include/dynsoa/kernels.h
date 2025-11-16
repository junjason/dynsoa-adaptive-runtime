// DynSoA Runtime SDK

#pragma once
#include "types.h"

namespace dynsoa {

using KernelFn = void (*)(ViewId, const KernelCtx&);

void begin_frame();
void run_kernel(const char* name, KernelFn fn, ViewId v, const KernelCtx& ctx);
void end_frame();

} // namespace dynsoa
