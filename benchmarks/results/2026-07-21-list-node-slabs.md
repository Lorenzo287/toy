# Experiment: List-node slabs

- Date: 2026-07-21
- Baseline commit: `1aa1069`
- Candidate: baseline plus the bounded list-node slab working tree
- OS / CPU: Windows 11 Pro / Intel Core i7-1065G7
- Compiler / version: GCC 16.1.0 (MSYS2 UCRT64)
- Build configurations: Release and AllocationStats
- Timing method: eight alternating baseline/candidate pairs, reversing process
  order between pairs
- Change under test: allocate persistent-list nodes from 128-node slabs and
  retain up to 64 KiB of completely empty slabs

| Measurement | Baseline | Candidate | Difference |
| --- | ---: | ---: | ---: |
| Complete `list.toy`: allocations | 3,615,913 | 700,545 | -80.6% |
| Complete `list.toy`: requested bytes | 122,524,773 | 75,776,227 | -38.2% |
| Repeated 5,000-node build/drop: allocations | 505,032 | 7,547 | -98.5% |
| Repeated 5,000-node build/drop: requested bytes | 12,446,397 | 10,868,523 | -12.7% |
| Repeated 1,000-node concat: allocations | 2,002,043 | 1,055 | -99.9% |
| Repeated 1,000-node concat: requested bytes | 48,125,633 | 167,807 | -99.7% |
| `cons front` median | 36.753 ms | 27.995 ms | -23.8% |
| `map and build` median | 84.465 ms | 62.909 ms | -25.5% |
| `concat copies-left` median | 95.408 ms | 12.269 ms | -87.1% |

The complete workload is `benchmarks/list.toy`. The focused build/drop workload
converts the same 5,000-value vector to a list 100 times. Its requested-byte
reduction is smaller because lists larger than the 64 KiB spare limit release
excess empty slabs and reacquire them on the next iteration. That bound avoids
retaining a transient list's entire high-water allocation.

The focused concat workload copies a 1,000-node left list 2,000 times. Its
working set fits within retained empty slabs after the first iteration, which
accounts for the larger allocation and timing improvement. All three timing
rows were faster in every alternating pair.

Focused regressions cover shared tails spanning many slabs and cache cleanup
while another Toy state still owns a multi-slab list. The complete GCC Debug,
Release, and LeakCheck suites pass, 62 tests each, including `core:ffi`.
