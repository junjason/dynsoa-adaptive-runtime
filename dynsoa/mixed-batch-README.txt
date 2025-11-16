
Mixed-kernel batch is now supported in tests/smoke_main.cpp with:
  --mix "physics,branchy,scatter,block/8"
  --csv path/to/results.csv

Example:
  ./dynsoa_smoke --entities 2000000 --frames 3000 --budget_us 1000 --dt 0.008                  --mix "physics,branchy,scatter,block/8"                  --csv bench/results.csv

Console output now uses std::cout with fixed precision.
