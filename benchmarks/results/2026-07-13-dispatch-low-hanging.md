# Experiment: Dispatch low-hanging candidates

- Date: 2026-07-13
- Commit: `d2b77d0`
- OS / CPU: Windows 11 / Intel Core i7-1065G7
- CMake configuration: Release
- Command: alternating direct executions of `benchmarks\dispatch.toy` across
  two baseline/candidate pairs per experiment

Two small candidates were tested and discarded:

1. Retaining the 64 lookup-cache object keys so cache hits could use pointer
   identity without verifying name bytes. One pair improved the dispatch total
   by 3.6%; the other regressed it by 3.0%.
2. Reusing a consumed numeric object with refcount one for binary arithmetic
   results. The two pairs swung by roughly 10% in opposite directions and did
   not show a repeatable improvement.

The machine was noisy, including isolated slow rows. Neither candidate cleared
the threshold for retaining a performance-only change. Future dispatch work
should begin with a more controlled profile or benchmark environment rather
than retrying these changes from timing alone.
