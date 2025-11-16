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

## Unity (optional)
Copy the built native library into:
```
Assets/DynSoA/Plugins/<platform>/dynsoa.*
```
Add `DynSoASmoke` to a scene and press Play.
CSV logs go to `Application.persistentDataPath/dynsoa_bench.csv`.
