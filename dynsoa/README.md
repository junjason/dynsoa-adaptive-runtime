
## Long-Run Demo & Smarter Adaptation

You can run longer experiments and let the scheduler learn with a UCB bandit.

```
# 10 minutes at 60 fps (~36k frames), 1M entities (adjust as needed)
export DYNSOA_FRAMES=2000
export DYNSOA_ENTITIES=1000000
export DYNSOA_VERBOSE=1
export DYNSOA_LEARN_LOG=learn_log.csv

./dynsoa_smoke
```

**What changed:**
- A simple **UCB1 bandit** picks among `{AoSoA(64|128|256), Matrix(64)}`.
- Reward = `realized_us - est_cost_us` per action (net improvement).
- Coefficients still learn from metrics; the bandit learns which transformation works for each view.
- `bench.csv` now appends layout labels to kernel names (e.g., `branchy_step_AoSoA`).
