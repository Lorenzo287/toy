# Experiment: Inline strings and direct result construction

- Date: 2026-06-23
- Commit: `3157d32` plus the current collection-review working tree
- OS / CPU: Windows 11 10.0.26200 / Intel Core i7-1065G7
- Compiler / version: Clang 22.1.3
- CMake configuration: Release
- Command: `.\benchmarks\run.ps1 -Benchmark string -Runs 5` (baseline), then
  `-Runs 10` (final candidate)
- Change under test: store short string/symbol payloads inside `tf_obj`, transfer
  owned buffers into results, and let exact-size producers fill final storage
  directly; geometrically reserve uniquely owned heap strings

The comparable workload median fell from 1168.311 ms to 924.278 ms wall time
(-20.9%). Raw Toy `clock` ticks are the median of five baseline and ten final
candidate processes.

| Workload | Baseline | Candidate |
| --- | ---: | ---: |
| indexed byte read short | 125 | 111 |
| indexed byte read long | 59 | 58 |
| reverse short | 76 | 62 |
| upper long | 53 | 53.5 |
| push-back growth n=10000 | 18 | 3 |
| push-back growth n=40000 | not measured | 12 |
| split into bytes | 441 | 350.5 |
| filter bytes | 202 | 160.5 |
| map identity bytes | 95 | 62.5 |

Wall-time samples ranged from 1013.740 to 1408.666 ms for the baseline and
820.503 to 1071.213 ms for the final ten-run candidate. The allocation-heavy
byte traversal workloads show the clearest improvement. Before geometric
reserve, increasing `push-back` growth from 10000 to 40000 bytes raised median
ticks from 13 to 187. With reserve, the corresponding medians were 3 and 12,
matching amortized linear growth.

The wall-time comparison used the identical eight-workload subset; the checked-in
benchmark retains the 40000-byte row as a scaling guard.
