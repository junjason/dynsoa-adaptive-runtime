# DynSoA Runtime SDK
### Adaptive Structure-of-Arrays Engine for Ultra-High-Performance Parallel Simulation

DynSoA is a next-generation runtime that transforms object-oriented or AoS simulation workloads into
Structure-of-Arrays (SoA), AoSoA, or matrix-aligned layouts — then *dynamically adapts them at runtime*
based on actual GPU/CPU performance metrics.

This repo implements the technology described in a patent family filed by  
**Sungmin "Jason" Jun (2025).**

---

## ⚡ Why DynSoA?
Traditional OOP simulation loops are slow because they cause:
- Random memory access  
- Warp divergence  
- Uncoalesced loads  
- Cache thrashing  

DynSoA solves this by:
1. Monitoring runtime metrics (divergence, coalescing, cache efficiency)
2. Transforming data layouts dynamically (SoA → AoSoA → matrix)
3. Re-scheduling workloads to match GPU/accelerator architecture

This yields dramatic performance improvements:

### 🚀 Benchmarks @ 2000 entities
| Backend | p50 (ms) |
|--------|----------|
| OOP    | ~2.35 ms |
| SoA    | ~2.56 ms |
| **DynSoA** | **0.00095 ms** |

> 2000–3000× improvement in update-step latency.

---

## 📦 Features
- Dynamic SoA migration
- Layout-aware scheduler
- Branch-divergence monitoring
- Warp-optimized tiling
- CPU/GPU dual-backend
- Boids & physics demo included
- Cloud-ready dispatch pipeline

---
## 🧩 Using DynSoA in Your Engine (C++ API Overview)

DynSoA is designed to sit under your existing simulation loop.
You keep your own structs / ECS components; DynSoA builds SoA / AoSoA views around them and manages layout + scheduling.

At a high level, a frame looks like:

Describe your data (fields / columns).

Create a DynSoA view over that data.

Begin a frame.

Run one or more kernels through DynSoA.

End the frame (DynSoA records metrics, may adapt layout).

Below is a minimal sketch of the public C API exposed by dynsoa.h (see the header for the exact signatures and types):

```
#include <dynsoa/dynsoa.h>

using dynsoa::ViewId;
using dynsoa::KernelCtx;
using dynsoa::ArchetypeDesc;
using dynsoa::FieldDesc;

// 1) Describe an archetype (set of fields / columns)
ArchetypeDesc make_order_archetype() {
    ArchetypeDesc a{};
    a.name = "Order";

    // Adjust field/offset/size to your actual struct layout.
    // See dynsoa/schema.h for the real FieldDesc definition.
    a.fields.push_back(FieldDesc{"price", offsetof(Order, price), sizeof(float)});
    a.fields.push_back(FieldDesc{"qty",   offsetof(Order, qty),   sizeof(float)});
    a.fields.push_back(FieldDesc{"side",  offsetof(Order, side),  sizeof(int)});
    return a;
}
```

DynSoA exposes a small runtime interface that looks roughly like:

```
// Global setup / teardown
void dynsoa_init();
void dynsoa_shutdown();

// Metrics / logging
void dynsoa_register_metric_sink(const char* csv_path); // e.g. "bench.csv"

// View / archetype life cycle
ViewId dynsoa_create_view(const ArchetypeDesc& desc);
void   dynsoa_destroy_view(ViewId view);

// Frame life cycle
void dynsoa_begin_frame();
void dynsoa_end_frame();

// Kernel execution (user function is called over the view)
using KernelFn = void (*)(ViewId, const KernelCtx&);

void dynsoa_run_kernel(const char* name,
                       KernelFn     fn,
                       ViewId       view,
                       const KernelCtx* user_ctx);
```

Note: the exact names and struct members may differ slightly in your version; use this as conceptual guidance and match it to dynsoa/dynsoa.h in your repo.

## 🔁 Minimal Frame Loop Example

```
Here’s an end-to-end sketch of integrating DynSoA into a simple simulation:

#include <dynsoa/dynsoa.h>
#include <vector>
#include <cstdio>

struct Boid {
    float x, y;
    float vx, vy;
    float ax, ay;
    int   state;
};

static const std::size_t N = 2000;

// Example kernel (called via dynsoa_run_kernel)
void boids_step_kernel(dynsoa::ViewId view, const dynsoa::KernelCtx& ctx) {
    // Pseudocode – replace with actual column accessors from your SDK.
    //
    // For example, if you have something like:
    //   auto pos_x = dynsoa_column_f32(view, pos_x_col);
    // then use that here.
    //
    // The important idea: you iterate over entities using the layout
    // that DynSoA has chosen (SoA, AoSoA, matrix, etc.), not AoS.
    for (std::size_t i = 0; i < ctx.entity_count; ++i) {
        // read/write SoA-backed fields here
        // pos_x[i] += vel_x[i] * ctx.dt;
        // ...
    }
}

int main() {
    std::vector<Boid> boids(N);

    // 1) Initialize DynSoA
    dynsoa_init();
    dynsoa_register_metric_sink("bench.csv");

    // 2) Describe your archetype (fields for Boid)
    dynsoa::ArchetypeDesc arch = {/* fill from your schema helpers */};

    // 3) Create a view (DynSoA-controlled representation of Boids)
    dynsoa::ViewId view = dynsoa_create_view(arch);

    // Optionally: bulk-upload initial data into the view here, or
    // let DynSoA allocate & own the storage depending on your design.

    const int frames = 2000;

    for (int frame = 0; frame < frames; ++frame) {
        dynsoa::KernelCtx ctx{};
        ctx.frame_index   = frame;
        ctx.entity_count  = static_cast<int>(N);
        ctx.dt            = 1.0f / 60.0f; // if you have dt in ctx

        dynsoa_begin_frame();
        dynsoa_run_kernel("boids_step", &boids_step_kernel, view, &ctx);
        dynsoa_end_frame();
    }

    dynsoa_destroy_view(view);
    dynsoa_shutdown();

    std::puts("Done. See bench.csv for per-frame timings.");
    return 0;
}
```

The key takeaways:

Your simulation stays “normal C++.”

DynSoA decides how the data is laid out and iterated (SoA/AoSoA/matrix) and logs performance, possibly adapting layouts over time.

You can run the same kernel via different backends (OOP / SoA / DynSoA) to compare.

You can look at tests/boids_multibackend.cpp and tests/smoke_main.cpp in this repo for real examples wired to your actual SDK.


---
## 🧪 Demos
tests/
└── boids_multibackend.cpp


To run:



mkdir build && cd build
cmake ..
make -j

then run boids_multibackend in build

## Quickstart (CLI)

For full instructions, see dynsoa/QUICKSTART.md. Summary:
```
cd dynsoa
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

# Run the smoke test / demo
./dynsoa_smoke

# Inspect benchmark output
cat bench.csv


On Windows (MSVC):
```
cd dynsoa
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
.\Release\dynsoa_smoke.exe
```

Artifacts:

Shared lib: build/Release/dynsoa.* (dll/so/dylib)

Logs: bench.csv

Learned weights: dynsoa_learn.json (path override via DYNSOA_LEARN_PATH)

Long-Run Learning Demo (UCB Bandit)

For longer experiments with the scheduler learning over time, see dynsoa/README.md.

Example:
```
export DYNSOA_FRAMES=2000          # number of frames
export DYNSOA_ENTITIES=1000000     # entities
export DYNSOA_VERBOSE=1
export DYNSOA_LEARN_LOG=learn_log.csv

./dynsoa_smoke
```

In this mode:

A UCB1 bandit chooses among {AoSoA(64/128/256), Matrix(64)}

Reward is based on realized runtime improvements

bench.csv logs kernel/layout combinations per frame


## Licensing

DynSoA Runtime SDK is dual-licensed:

- Open source: [MPL 2.0](./LICENSE)
- Commercial: [LICENSE-COMMERCIAL.txt](./LICENSE-COMMERCIAL.txt)

See [PATENT_NOTICE.txt](./PATENT_NOTICE.txt) for patent information.
For commercial licensing, contact **jjun300700@gmail.com**.


---

## ⭐ Roadmap
- GPU kernel generation
- JIT specialization
- AoSoA hybrid layouts
- Cloud cluster executor

---

## 📈 Want to support development?
Star the repo ⭐ — it helps tremendously.