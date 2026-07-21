# Experiment: Immediate tagged integers

- Date: 2026-07-21
- Baseline commit: `5891eae`
- Candidate: baseline plus the immediate-integer working tree
- OS / CPU: Windows 11 Pro / Intel Core i7-1065G7
- Compiler / version: GCC 16.1.0 (MSYS2 UCRT64)
- Build configurations: Release and AllocationStats
- Timing method: eight alternating baseline/candidate pairs, reversing process
  order between pairs
- Change under test: encode signed 63-bit integers in `tf_obj *` values on
  64-bit targets and retain boxed storage for full-width outliers and parsed
  source literals

| Measurement | Baseline | Candidate | Difference |
| --- | ---: | ---: | ---: |
| `binrec` Fibonacci(32): allocations | 106 | 44 | -58.5% |
| `binrec` Fibonacci(32): requested bytes | 37,247 | 32,783 | -12.0% |
| 200,000-value `range`: allocations | 200,022 | 22 | -99.99% |
| 200,000-value `range`: requested bytes | 16,516,872 | 2,116,872 | -87.2% |
| `binrec` Fibonacci(32) median | 2,677.442 ms | 2,518.881 ms | -5.9% |
| 5,000,000 scalar increments median | 210.744 ms | 200.103 ms | -5.0% |
| 20 x 200,000-value `range` median | 401.387 ms | 24.906 ms | -93.8% |

The Fibonacci workload is
`32 [ 2 < ] [ ] [ pred dup pred ] [ + ] binrec drop`. Its timings were
thermally noisy: the candidate was faster in five of eight pairs, so the median
is directional rather than a precise estimate. The scalar increment candidate
was faster in six of eight pairs. The range candidate was faster in every pair
because it no longer allocates one `tf_obj` for each generated integer.

The immediate range is -2^62 through 2^62-1. Values outside it use the existing
boxed representation, and 32-bit builds continue to box every integer. Parsed
literals also remain boxed to retain source spans. Regression tests cross both
tag boundaries, exercise boxed/immediate equality and hashing in collections,
execute a dynamically constructed quotation, and retain both representations
through the public C API.

The complete GCC Debug, Release, and LeakCheck suites pass, 62 tests each,
including `core:ffi`.
