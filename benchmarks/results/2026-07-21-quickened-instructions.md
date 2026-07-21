# Experiment: Quickened call instructions

- Date: 2026-07-21
- Baseline commit: `9e50275`
- Candidate: baseline plus the quickened-instruction working tree
- OS / CPU: Windows 11 Pro / Intel Core i7-1065G7
- Compiler / version: GCC 16.1.0 (MSYS2 UCRT64)
- Build configurations: Release and AllocationStats
- Timing method: eight alternating baseline/candidate pairs, reversing process
  order between pairs
- Change under test: cache resolved dictionary indexes by quotation/package/PC,
  then execute common `dup`, `pred`, integer `+`, integer `*`, and integer `<`
  paths directly in the VM

| Measurement | Baseline | Candidate | Difference |
| --- | ---: | ---: | ---: |
| Fibonacci(32) process median | 2,237.211 ms | 1,877.680 ms | -16.1% |
| Fibonacci(32) allocations | 44 | 48 | +4 |
| Fibonacci(32) requested bytes | 32,783 | 33,799 | +3.1% |
| Inline native (`+`) median | 1,132.190 ms | 950.669 ms | -16.0% |
| User word (`inc-loc`) median | 1,409.602 ms | 1,216.427 ms | -13.7% |
| Inline native (`dup *`) median | 1,328.848 ms | 933.866 ms | -29.7% |
| User word (`square-loc`) median | 1,577.675 ms | 1,211.075 ms | -23.2% |
| Comparable dispatch total median | 5,660.388 ms | 4,363.228 ms | -22.9% |

The Fibonacci workload is
`32 [ 2 < ] [ ] [ pred dup pred ] [ + ] binrec drop`. The candidate was faster
in every alternating pair. The dispatch rows come from
`benchmarks/dispatch.toy`; its candidate process total was faster in seven of
eight pairs. Heavy background load caused isolated multi-second spikes in both
sets, so medians and alternating comparisons are more meaningful than absolute
times.

Each program frame acquires a bounded context-local sidecar whose entries are
indexed directly by program counter. A slot starts as a general call, retains
the resolved dense dictionary index, and is guarded by the existing resolution
generation. Native replacement, user redefinition, visibility changes,
package-state changes, and imports invalidate specialized slots. Original
quotation objects and source spans remain untouched.

The specialized operations handle only their exact fast path. Mixed numeric
types, stack errors, and integer overflow call the original native function.
Four one-time sidecar allocations account for the small Fibonacci allocation
increase; recursive execution performs no new per-node allocation.

The complete GCC Debug, Release, and LeakCheck suites pass, 62 tests each,
including the `core:ffi` package integration test.
