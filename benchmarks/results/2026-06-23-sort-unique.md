# Experiment: Hybrid sort and hash-assisted unique

- Date: 2026-06-23
- Commit: `2bca365` plus the current collection-review working tree
- OS / CPU: Windows 10.0.26200.0 / Intel Core i7-1065G7
- Compiler / version: Clang 22.1.3
- CMake configuration: Release
- Command: `.\benchmarks\run.ps1 -Benchmark sequence-algorithms -Runs 3`
- Change under test: replace fixed-pass bubble sort with insertion/merge and
  byte-counting hybrids; add delayed hashing to `unique`

The comparable suite median fell from 203.510 ms to 50.104 ms wall time
(-75.4%). Raw Toy `clock` ticks below are medians of three fresh processes.

| Workload | Baseline | Candidate |
| --- | ---: | ---: |
| sort vector sorted n=8 | 7 | 6 |
| sort vector reverse n=32 | 9 | 4 |
| sort vector permuted n=128 | 11 | 2 |
| sort vector permuted n=512 | 25 | 2 |
| sort vector reverse n=2048 | 16 | 1 |
| sort list reverse n=512 | 7 | 1 |
| unique distinct vector n=16 | 10 | 9 |
| unique distinct vector n=128 | 35 | 5 |
| unique distinct vector n=1024 | 42 | 1 |
| unique dense vector n=1024 | 4 | 2 |
| unique distinct list n=512 | 11 | 1 |

Cutoff sweep medians for the full comparable suite:

| Insertion / string / hash cutoff | Wall median |
| --- | ---: |
| 8 / 32 / 8 | 52.458 ms |
| 16 / 64 / 16 | 50.104 ms |
| 32 / 128 / 32 | 58.446 ms |

The selected cutoffs are implementation details, not language guarantees.
String sub-millisecond measurements were below the resolution of Toy's
`clock`; the wall-time suite and correctness tests cover those paths.
