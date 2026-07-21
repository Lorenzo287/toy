# Experiment: Retained word-resolution cache

- Date: 2026-07-21
- Commit: `168f812` plus the existing working tree
- OS / CPU: Windows 11 Pro / Intel Core i7-1065G7
- Compiler / version: GCC 16.1.0 (MSYS2 UCRT64)
- Build configuration: Release
- Command: `nob benchmark dispatch --runs 3 --toy
  build/gcc/release/toy.exe` before and after the change
- Change under test: add a 64-entry direct-mapped cache from retained
  call/symbol objects and lexical packages to stable dense dictionary indexes,
  guarded by a dictionary-resolution generation

| Workload | Baseline | Candidate | Difference |
| --- | ---: | ---: | ---: |
| Inline native (`+`) | 1,564.117 ms | 1,116.126 ms | -28.6% |
| User word (`inc-loc`) | 2,331.960 ms | 1,387.741 ms | -40.5% |
| Inline native (`dup *`) | 1,786.507 ms | 1,359.964 ms | -23.9% |
| User word (`square-loc`) | 3,016.875 ms | 1,607.510 ms | -46.7% |
| Comparable dispatch total | 8,699.460 ms | 5,471.340 ms | -37.1% |
| Process wall median | 8,935.886 ms | 5,822.510 ms | -34.8% |

The cache retains each key object, preventing released `tf_obj` storage from
being reused at the same address while an entry remains live. Lexical package
is part of the key. Inserted definitions, visibility changes, package-state
changes, and import changes advance a resolution generation; redefinition can
reuse the same stable entry index and therefore observes the replacement
implementation directly.

The three-run timings varied substantially, so the per-row percentages should
be treated as directional. Every candidate process was nevertheless faster
than every baseline process by wall time. Regressions cover redefinition,
more-than-capacity dynamic call objects and eviction, root-native shadowing,
and the same symbol object resolving in two lexical packages. The complete GCC
Release and LeakCheck suites pass, 62 tests each, including `core:ffi`.
