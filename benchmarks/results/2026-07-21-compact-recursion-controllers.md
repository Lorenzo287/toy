# Experiment: Compact recursion controllers

- Date: 2026-07-21
- Baseline commit: `941a188`
- Candidate: baseline plus the compact `binrec`, `genrec`, and `linrec`
  controller working tree
- OS / CPU: Windows 11 Pro / Intel Core i7-1065G7
- Compiler / version: GCC 16.1.0 (MSYS2 UCRT64)
- Build configurations: Release and AllocationStats
- Timing method: eight alternating baseline/candidate pairs, reversing process
  order between pairs
- Change under test: retain recursion callables and predicate state once, use
  compact logical levels for `binrec`, and use one pending-unwind count for
  `genrec` and `linrec`

| Measurement | Baseline | Candidate | Difference |
| --- | ---: | ---: | ---: |
| Fibonacci(32) process median | 2,205.516 ms | 1,879.858 ms | -14.8% |
| Fibonacci(32) allocations | 133 | 106 | -20.3% |
| Fibonacci(32) requested bytes | 52,649 | 33,191 | -37.0% |
| Deep `genrec` + `linrec` allocations | 9,925 | 42 | -99.6% |
| Deep `genrec` + `linrec` requested bytes | 6,648,052 | 21,714 | -99.7% |
| 200,000 `genrec` factorials | 565.999 ms | 548.268 ms | -3.1% |
| 200,000 `linrec` factorials | 582.248 ms | 560.769 ms | -3.7% |

The Fibonacci timing ran
`32 [ 2 < ] [ ] [ pred dup pred ] [ + ] binrec drop`. The candidate was faster
in all eight alternating pairs despite substantial machine-level timing
variance. The linear-recursion timings use Toy's `monotonic-ns` inside each
process; their smaller improvements should be treated as directional.

The deep allocation workload runs both schemes to depth 5,000. Previously each
logical level retained the same four quotations, owned a predicate evaluator,
acquired a 512-byte cached control-state block, and pushed a native VM frame.
The candidate retains that common state once. Binary recursion keeps six levels
inline, stores the common one-value rollback directly in a level, and grows
larger logical-level and rollback buffers geometrically.

Focused regressions cover 5,000-level binary, linear, and general recursion,
multi-value ambient-stack rollback, and error cleanup after the binary level
buffer grows. The complete GCC Release and LeakCheck suites pass, 62 tests
each, including `core:ffi`.
