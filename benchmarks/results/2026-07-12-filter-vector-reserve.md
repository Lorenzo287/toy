# Experiment: Bounded reserve for vector filter results

- Date: 2026-07-12
- Commit: `799475c` plus the word-lookup-cache working tree
- OS / CPU: Windows 11 Pro 10.0.26200 / Intel Core i7-1065G7
- Compiler / version: Clang 22.1.3
- CMake configuration: Release and AllocationStats
- Command: isolated 2000-iteration vector-filter workload under
  AllocationStats; 15 alternating Release baseline/candidate pairs using the
  `filter with ambient stack` row from `benchmarks/runtime-internals.toy`
- Change under test: after a vector filter result fills its two inline slots,
  reserve up to 64 slots based on input length before continuing geometric
  growth

| Measurement | Baseline | Candidate | Difference |
| --- | ---: | ---: | ---: |
| Focused filter allocation calls | 12,292 | 4,292 | -65.1% |
| Focused filter reallocations | 10,001 | 2,001 | -80.0% |
| Focused filter requested bytes | 4,068,547 | 3,108,514 | -23.6% |
| Runtime-internals allocation calls | 12,407 | 4,407 | -64.5% |
| Runtime-internals requested bytes | 4,086,290 | 3,126,290 | -23.5% |
| Filter time median | 156.662 ms | 154.404 ms | -1.4% |

The reserve is delayed until a third match, so empty, one-item, and two-item
results retain inline storage and allocate no vector element array. The reserve
is capped at 64 slots, bounding unused capacity for sparse results independently
of input size. Dense results continue growing geometrically after that point.

Timing on this machine was noisy in both directions. The paired in-language
median is effectively neutral, so this change is retained for the measured
allocator-call and cumulative-requested-byte reductions rather than a speed
claim. The full Release and LeakCheck suites pass, and generated builtin files
remain current.
