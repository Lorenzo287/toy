# Experiment: Inline binrec rollback snapshots

- Date: 2026-07-21
- Commit: `27a1c9d` plus the existing working tree
- OS / CPU: Windows 11 Pro / Intel Core i7-1065G7
- Compiler / version: Clang 22.1.3
- Build configuration: Release and AllocationStats
- Command: five direct executions of
  `32 [ 2 < ] [ ] [ pred dup pred ] [ + ] binrec drop`; one execution under
  AllocationStats
- Change under test: store up to 32 retained rollback-stack pointers inside
  each binrec state, with heap allocation when the snapshot is larger

| Measurement | Baseline | Candidate | Difference |
| --- | ---: | ---: | ---: |
| Release time median | 3.070 s | 2.270 s | -26.1% |
| Allocation calls | 3,524,711 | 134 | -99.996% |
| Requested bytes | 245,952,740 | 50,668 | -99.979% |

The inline array increases `binrec_state` from 224 to 480 bytes, so it still
fits in the existing 512-byte cached control-state block. The measured
Fibonacci workload stays within the inline capacity; unusually deep ambient
stacks retain the previous heap-backed behavior.

Focused Release and LeakCheck runs cover the normal path, the heap fallback,
and error cleanup while a fallback snapshot is active. The Toy executable
builds successfully, but the repository-wide Nob suite could not start because
the local Windows build is missing the pre-existing libffi development header
`ffi.h`.
