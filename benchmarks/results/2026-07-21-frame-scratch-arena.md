# Experiment: Frame-owned scratch arena

- Date: 2026-07-21
- Baseline commit: `747db85`
- Candidate: baseline plus the frame-owned scratch arena working tree
- OS / CPU: Windows 11 Pro / Intel Core i7-1065G7
- Compiler / version: GCC 16.1.0 (MSYS2 UCRT64)
- Build configurations: Release and AllocationStats
- Timing method: eight alternating baseline/candidate pairs, reversing process
  order between pairs
- Change under test: replace exact heap fallbacks for oversized control and
  predicate stack snapshots with strict-LIFO, per-context scratch storage

| Measurement | Baseline | Candidate | Difference |
| --- | ---: | ---: | ---: |
| 10,000 oversized control snapshots: allocations | 10,108 | 109 | -98.9% |
| 10,000 oversized control snapshots: requested bytes | 5,149,693 | 33,885 | -99.3% |
| 10,000 oversized predicate snapshots: allocations | 10,133 | 134 | -98.7% |
| 10,000 oversized predicate snapshots: requested bytes | 5,231,566 | 35,758 | -99.3% |
| 100,000 oversized control snapshots: median | 81.900 ms | 61.352 ms | -25.1% |
| 100,000 oversized predicate snapshots: median | 1,335.033 ms | 1,361.642 ms | +2.0% |

The control allocation workload repeatedly runs `dip` with 64 surrounding
values. The predicate workload repeatedly runs `some` over 16 values with the
same 64-value surrounding stack. The timing comparison uses the corresponding
100,000-iteration workloads. Control was faster in seven of eight alternating
pairs. Predicate timing remained noisy and shows no repeatable speed benefit;
its result here is removal of allocator traffic rather than lower elapsed time.

The arena starts with a 4 KiB block, grows by linking another block when nested
snapshots require it, and rewinds in native-frame cleanup order. Released
blocks are reusable up to a 64 KiB per-context spare-byte limit. Larger or
excess blocks are freed immediately so a single unusually deep stack does not
become a permanent high-water allocation.

Focused regressions cover nested oversized control snapshots, error unwinding,
and oversized predicate restoration. The complete GCC Debug, Release, and
LeakCheck suites pass, 62 tests each, including `core:ffi`.
