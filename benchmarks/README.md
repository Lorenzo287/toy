# Benchmarks

This directory contains performance experiments, not correctness tests. Keep
workloads deterministic and focused enough that a result can be tied to one
implementation choice.

Build an optimized interpreter and run the full suite:

```powershell
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\benchmarks\run.ps1
```

Run one workload or use another executable:

```powershell
.\benchmarks\run.ps1 -Benchmark vector -Runs 10
.\benchmarks\run.ps1 -Toy .\build-prof\toy.exe -Benchmark dispatch
```

The Toy scripts print `clock` CPU ticks for individual operations. The runner
also reports wall time for each fresh process and its median. Compare results
only across the same machine, compiler, build configuration, and workload.
Before drawing a conclusion:

1. Record the commit, compiler/version, build type, OS, CPU, and command.
2. Run enough samples to see normal variance; do not select the fastest run.
3. Change one implementation technique at a time.
4. Confirm behavior and leak tests separately; a benchmark is not a regression
   test.
5. Store durable measurements from meaningful experiments under `results/`
   using the provided template.

Current workloads:

- `dispatch.toy`: inline native calls versus user-word dispatch.
- `vector.toy`: unique `push-back`, non-shrinking `pop-back`, indexed reads,
  and unique/shared-left `concat`.
