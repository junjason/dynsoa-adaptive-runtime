# DynSoA (Learning Runtime) — Quickstart

## Build (CLI)

### macOS
```bash
brew install cmake
cd dynsoa
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
./dynsoa_smoke
open bench.csv
```

### Linux
```bash
sudo apt-get install -y build-essential cmake
cd dynsoa
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
./dynsoa_smoke
```

### Windows (MSVC)
```bat
cd dynsoa
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
.\Release\dynsoa_smoke.exe
```

Artifacts:
- Shared lib: `build/Release/dynsoa.*` (dll/so/dylib)
- Logs: `bench.csv`
- Learned weights persisted to `dynsoa_learn.json` in CWD (override by `DYNSOA_LEARN_PATH` env var).

## Runtime Controls

The smoke and boids demos can be steered without recompiling, using environment variables:

```bash
# Number of simulation frames to run
export DYNSOA_FRAMES=2000

# Number of entities (boids / rows) to spawn
export DYNSOA_ENTITIES=500000

# Enable verbose scheduler logging (layout decisions, bandit signals)
export DYNSOA_VERBOSE=1

# Optional: where to write learning traces
export DYNSOA_LEARN_LOG=learn_log.csv

# Optional: override default path for persisted bandit weights
export DYNSOA_LEARN_PATH=dynsoa_learn.json
```

Outputs:
- `bench.csv` — per-frame timing snapshots and layout selections
- `dynsoa_learn.json` — persisted runtime weights, re-used across runs


## Unity (optional)
Copy the built native library into:
```
Assets/DynSoA/Plugins/<platform>/dynsoa.*
```
Add `DynSoASmoke` to a scene and press Play.
CSV logs go to `Application.persistentDataPath/dynsoa_bench.csv`.
