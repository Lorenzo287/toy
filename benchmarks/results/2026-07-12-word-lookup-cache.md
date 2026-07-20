# Experiment: Resolved word lookup cache

- Date: 2026-07-12
- Commit: `799475c` plus the word-lookup-cache working tree
- OS / CPU: Windows 11 Pro 10.0.26200 / Intel Core i7-1065G7
- Compiler / version: Clang 22.1.3
- CMake configuration: Release; Profile for sampling
- Command: alternating direct executions of `benchmarks/dispatch.toy` across
  five baseline/candidate pairs; then
  `nob benchmark runtime-internals --runs 10 --toy path/to/toy` for each binary
- Change under test: add a 64-entry direct-mapped cache from call/symbol object
  addresses to stable dense dictionary entry indexes, with name verification
  on hits

| Workload | Baseline | Candidate | Difference |
| --- | ---: | ---: | ---: |
| Inline native (`+`) | 1547.097 ms | 1410.023 ms | -8.9% |
| User word (`inc-loc`) | 2056.437 ms | 1896.028 ms | -7.8% |
| Inline native (`dup *`) | 1794.185 ms | 1653.653 ms | -7.8% |
| User word (`square-loc`) | 2452.085 ms | 2058.220 ms | -16.1% |
| Comparable dispatch total | 7933.684 ms | 6966.448 ms | -12.2% |
| Runtime-internals wall median | 429.578 ms | 406.783 ms | -5.3% |

Dispatch rows are medians from five executions. The comparable total is the
median of each process's four measured rows, so it preserves within-process
timing conditions. Baseline and candidate order alternated between pairs.

Samply profiles used the Profile build at 1000 Hz. Before the change,
`tf_dict_lookup` was present in 37.9% of samples and `dict_hash` in 6.1%. With
the cache, lookup fell to 21.2% and hashing was below the sampling threshold;
the remaining lookup cost includes cache selection and name verification.

The machine showed substantial timing variance, including isolated slow first
runs. The alternating dispatch result and the separate runtime-internals
improvement both support retaining the change. Focused cache regressions cover
repeated execution, redefinition, and released-object address reuse. The full
Release and LeakCheck suites pass.
