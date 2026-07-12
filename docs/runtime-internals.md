# Runtime Internals

Toy exposes value semantics at the language level, but the runtime uses several
specialized layouts to avoid unnecessary allocation, copying, and C recursion.

## Execution Model

The VM is iterative. User words and native callable runners push frames onto an
explicit call stack; the main VM loop executes those frames until the stack is
empty.

- Program frames point at vector quotations and keep a program counter.
- Program traversal dispatches explicit call nodes; symbols are always pushed
  as inert name values unless a word such as `exec` invokes them explicitly.
- Native continuation frames resume C native words after a callable has run.
- New native words that execute user code should schedule frames or
  continuations, not call `tf_vm_exec()` recursively.
- Program and native payloads share a union because a frame is one or the other.
- Program frames keep one capture binding inline and allocate a small binding
  table only when more captures are introduced.

This model keeps Toy-level recursion and higher-order combinators independent
from the C call stack.

## Object Layout

Runtime values are boxed `tf_obj` records with reference counts. Collections
store retained `tf_obj *` references, so most collection transformations move
or share object references rather than deep-copying values.

Important object-level optimizations:

- vectors keep two elements inline inside the object before allocating an
  external element array;
- short strings, symbols, and call nodes store bytes inline inside the object;
- heap strings reuse otherwise idle inline bytes to remember capacity;
- released `tf_obj` records can be reused through a bounded object cache;
- many update-style words mutate only when `refcount == 1`, otherwise they
  clone first to preserve value semantics.

These choices are intentionally invisible to Toy code.

## Collection Layouts

### Vectors

Vectors are arrays and are also the quotation representation. They use inline
storage for very small vectors, geometric reserve for repeated growth, and
copy-on-write for update-style words such as `set-at`, `push-back`,
`pop-back`, and `concat`.

Back pops reduce length but do not shrink capacity. That keeps vector-as-stack
workloads amortized O(1) instead of paying reallocation costs on alternating
push/pop patterns.

### Strings and Symbols

Strings are flat byte arrays. Short values are inline; longer values use one
heap allocation. Exact-size producers allocate the final string once, while
dynamic producers grow geometrically and transfer their buffer into the final
object.

Indexed byte reads are O(1), but they return a new one-byte string because a
Toy character is itself a string value.

### Lists

Lists are immutable singly linked values. A list wrapper caches length, and
nodes are independently refcounted, so `rest` and `uncons` can return a new
wrapper that shares the suffix. This makes front operations cheap while keeping
list values persistent.

### Maps and Sets

Maps and sets use two coordinated arrays:

- a dense insertion-order entry array;
- an open-addressed bucket array storing one-based entry indexes.

The dense array gives deterministic projection/display order. The bucket array
gives expected O(1) lookup. Removing a present entry shifts the dense array and
rebuilds bucket indexes, which is why ordered removal is O(n).

### Deques

Deques use a circular buffer. The head index can move without shifting
elements, so front and back pushes/pops are amortized O(1). Shared deques are
cloned before updates.

### Priority Queues

Priority queues use a binary min-heap. Each entry stores:

- the original priority object;
- a monotonic insertion order for stable equal-priority behavior;
- the value object.

`>pqueue` reserves once and heapifies bottom-up in O(n). `pairs` clones and
pops the heap to emit public priority order without exposing the internal heap
layout.

## Source Locations

Parsed values retain compact source spans. All spans from one parse share a
refcounted source-file record, so a filename is allocated once rather than once
per token. Program and native frames borrow spans from executable values while
the owning program remains active.

## Builtin Metadata

Builtin metadata is generated from `builtins.json`.

Generated outputs include:

- grouped native registration tables in `src/tf_builtins.inc`;
- runtime docs in `src/tf_docs.c`;
- REPL builtin declarations in `src/tf_repl_builtins.inc`;
- README builtin tables;
- LSP builtin docs;
- Tree-sitter and VS Code builtin word lists.

This keeps help output, documentation, editor tooling, and native registration
from drifting apart.

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

Use the benchmark suite rather than isolated impressions:

```powershell
cmake -S . -B build-alloc -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DBUILD_MODE=AllocationStats
cmake --build build-alloc
.\benchmarks\run.ps1 -Toy .\build-alloc\toy.exe
```

`BUILD_MODE=AllocationStats` reports checked allocation calls and cumulative
requested bytes. These totals compare identical workloads; they are not live or
peak memory. Timing and allocation workloads live in `benchmarks/`, including
`runtime-internals.toy` for continuations, captures, predicate snapshots, and
recursion schemes.
