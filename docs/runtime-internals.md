# Runtime Internals

Toy uses specialized internal storage where boxed language values would add
cost or weaken invariants.

## Interpreter State

- The data stack is an exclusively owned Toy vector and therefore benefits
  from inline vector storage and geometric growth.
- The execution stack is a geometric array of typed program/native frames.
  Program and native payloads share a union because they are mutually
  exclusive. Program frames keep one capture binding inline and allocate a
  small table only when further bindings are introduced.
- The global word dictionary stores definitions densely and uses a separate
  open-addressed array of one-based entry indexes. Native names reference the
  static builtin catalog; user-defined names own compact string copies.

These structures should not be replaced with public maps, vectors, or deques
when doing so would require boxing C function pointers or exposing persistence
semantics that the VM does not need.

## Source Locations

Parsed values retain compact source spans. All spans from one parse share a
refcounted source-file record, so a filename is allocated once rather than once
per token. Program and native frames borrow spans from executable values while
the owning program remains active.

## Bounded Reuse

The runtime keeps two bounded freelists:

- up to 256 released `tf_obj` records;
- up to 128 continuation-state blocks of 512 bytes.

Both caches are drained before allocation/leak reports. The bounds prevent a
short-lived high-water workload from retaining unbounded process memory while
removing allocator traffic from normal scalar execution and combinator loops.

Predicate continuations keep up to 16 surrounding stack references inline and
fall back to an exact heap snapshot for deeper stacks. Collection predicates
reuse an invariant surrounding-stack snapshot across iterations.

## Measuring Changes

Configure `BUILD_MODE=AllocationStats` to report checked allocation calls and
cumulative requested bytes. These totals compare identical workloads; they are
not live or peak memory. Timing workloads live in `benchmarks/`, including
`runtime-internals.toy` for continuations, captures, predicate snapshots, and
recursion schemes.
