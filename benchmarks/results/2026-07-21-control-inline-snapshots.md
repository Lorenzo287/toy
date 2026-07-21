# Experiment: Shared inline control-stack snapshots

- Date: 2026-07-21
- Commit: `27a1c9d` plus the existing working tree
- OS / CPU: Windows 11 Pro / Intel Core i7-1065G7
- Compiler / version: Clang 22.1.3
- Build configuration: Release and AllocationStats
- Command: isolated repeated `treerec`, `map`, `genrec`, and `linrec`
  workloads with eight ambient stack values
- Change under test: use 32-pointer inline storage with heap fallback for
  rollback snapshots owned by cached control-combinator states

| Measurement | Baseline | Candidate | Difference |
| --- | ---: | ---: | ---: |
| `treerec`, 10,000 calls: allocations | 120,070 | 10,070 | -91.6% |
| `treerec`, 10,000 calls: requested bytes | 7,385,196 | 345,196 | -95.3% |
| `map`, 20,000 calls: allocations | 40,061 | 20,061 | -49.9% |
| `map`, 20,000 calls: requested bytes | 1,941,772 | 661,772 | -65.9% |
| `genrec`, 20,000 calls: allocations | 60,084 | 60,084 | unchanged |
| `linrec`, 20,000 calls: allocations | 40,079 | 40,079 | unchanged |

`treerec` previously allocated one rollback array per visited tree value when
the surrounding stack was nonempty. Its remaining allocations in this workload
come primarily from constructing mapped vector nodes. `map` loses one snapshot
allocation per invocation; its result objects and backing arrays remain.

The unchanged `genrec` and `linrec` counts confirm that they already use the
predicate evaluator's inline snapshot and do not own this form of rollback
copy. All enlarged states have compile-time checks that keep them inside the
existing 512-byte control-state cache block.

Focused Release and LeakCheck runs cover normal control behavior, deep-stack
heap fallback, recursive combinators, and error cleanup. Timing was noisy on
this machine, so this experiment records only the repeatable allocation and
cumulative-requested-byte reductions. The repository-wide Nob suite remains
blocked by the local Windows installation's missing libffi header `ffi.h`.
