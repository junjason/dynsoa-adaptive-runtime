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

## 🧪 Demos
tests/
├── boids_multibackend.cpp
└── smoke_main.cpp


To run:



mkdir build && cd build
cmake ..
make -j
./demos/boids/boids_demo

## Quickstart (CLI)

For full instructions, see dynsoa/QUICKSTART.md. Summary:

cd dynsoa
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Run the smoke test / demo
./dynsoa_smoke

# Inspect benchmark output
cat bench.csv


On Windows (MSVC):

cd dynsoa
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
.\Release\dynsoa_smoke.exe


Artifacts:

Shared lib: build/Release/dynsoa.* (dll/so/dylib)

Logs: bench.csv

Learned weights: dynsoa_learn.json (path override via DYNSOA_LEARN_PATH)

Long-Run Learning Demo (UCB Bandit)

For longer experiments with the scheduler learning over time, see dynsoa/README.md.

Example:

export DYNSOA_FRAMES=2000          # number of frames
export DYNSOA_ENTITIES=1000000     # entities
export DYNSOA_VERBOSE=1
export DYNSOA_LEARN_LOG=learn_log.csv

./dynsoa_smoke


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