# Runtime Internals

Toy exposes value semantics at the language level, but the runtime uses several
specialized layouts to avoid unnecessary allocation, copying, and C recursion.

## Execution Model

Parsed Toy code is a vector. The VM reads that vector from left to right. A
literal value is pushed onto the data stack; a call instruction looks up and
runs a word; a nested vector is pushed as data until `exec` or another
combinator asks to run it.

The runtime must remember where to continue whenever one word calls another.
It records that work in *frames* on an explicit VM stack instead of relying on
the C call stack. The main loop keeps processing frames until no work remains.

- Program frames point at vector quotations and keep a program counter.
- Program frame kinds distinguish roots, quotations, and user words without
  increasing frame size. Debugger frame views recover direct-call names from
  callers and otherwise resolve user bodies through the dictionary.
- Program traversal dispatches call instructions. Symbols are pushed as name
  values; a word such as `exec` may later use a symbol to choose code to run.
- Resolved call and symbol objects use a 64-entry direct-mapped lookup cache.
  Cache entries store stable dense dictionary indexes rather than pointers into
  the movable entry array. Hits verify the current name bytes because released
  objects may later reuse the same address; redefining a word updates its
  existing indexed entry and remains visible through the cache.
- Native continuation frames resume C native words after a callable has run.
- New native words that execute user code should schedule frames or
  continuations, not call `tf_vm_exec()` recursively.
- Program and native payloads share a union because a frame is one or the other.
- Program frames keep one capture binding inline and allocate a small binding
  table only when more captures are introduced.

This model keeps Toy-level recursion and higher-order combinators independent
from the C call stack.

## Module Lookup

`require` records source modules in a per-context registry and schedules their
programs on the existing VM frame stack. A registry entry is marked loading,
loaded, or failed, which provides load-once behavior and detects cyclic
dependencies without recursively entering the VM.

The dictionary remains one hash table. Module definitions are stored under
canonical `module.word` keys, while each program frame carries its lexical
module. Unqualified lookup checks that module first and then the root native
words; qualified lookup exposes only explicitly exported words from loaded
modules. A separate alias table maps a short prefix to a module index within one
importing module, so aliases do not copy or rename dictionary entries.
Quotations retain their shared source-file record, allowing an escaped
quotation to recover its lexical module when it is executed later. Raw `load`
marks the parsed source with the caller's module. Relative `load` and `require`
paths try that source file's directory before the process working directory.
If no source module exists, `require` searches for a versioned shared native
module in the requiring source directory, `TOY_MODULE_PATH`, and the working
directory. Descriptor-registered native modules enter the same registry already
marked as loaded, and their exported native words use the same scoped
dictionary entries as source-module exports. Loaded library handles stay in the
context until final teardown, after stacks, frames, definitions, and resource
destructors have been released.

## Embedding Boundary

The core sources build as the static `toy_runtime` library. The `toy`
executable links that library and keeps command-line parsing, the REPL,
linenoise, the terminal debugger frontend, and the debug protocol outside the
runtime target.

`include/toy.h` is the experimental public boundary. It treats the interpreter
state as opaque and exposes evaluation, host-to-Toy word calls, synchronous
native word/module registration, primitive stack access, persistent value
references, basic collection access, diagnostics, and interruption. Persistent
values retain their internal object but remain state-bound, so C cannot expose
or transfer `tf_obj` layouts between runtimes. Typed resource access wraps
external pointers in ordinary refcounted objects with copied tags and
exactly-once destructors, while keeping the pointer and object layout opaque to
Toy code. `include/toy_module.h`
defines shared-module ABI version 1: an exported descriptor entry point and a
size-tagged host function table. `toy_module_support` forwards the familiar
public stack/resource calls through that table, so a plugin does not link a
second runtime.
Internal headers continue to expose implementation structures only to the
runtime and bundled frontends. See the [embedding guide](./embedding.md) for
the current ownership and execution contracts.

The optional `modules/ffi/` module is a consumer of this boundary rather than
part of the VM. It represents loaded libraries and prepared libffi call
interfaces as typed resources. A prepared function retains its library; Toy
resource teardown releases the call metadata and closes the foreign library
before the native `ffi` module itself is unloaded.

Generated bindings take the other route through the same module boundary.
`tools/generate-binding.js` emits ordinary native callbacks that perform
range-checked stack conversion and direct C calls. The resulting module links
`toy_module_support`, so generated code still shares the host VM without
linking another runtime.

## Debugger Hooks

The VM can install a frontend-neutral debug hook on a context. Before each Toy
instruction, the hook receives the instruction, source span, program counter,
and frame depth. It returns one of three actions:

- step executes that instruction and pauses before the next Toy instruction;
- continue suppresses further pauses for the current VM invocation;
- abort unwinds the invocation with interruption cleanup.

Native words remain atomic at this boundary. Native continuations are visible
in read-only frame views, and any Toy quotations they schedule produce normal
instruction events. Frame views expose stable word labels, PCs, program lengths,
call sites, current locations, and borrowed capture bindings without giving
frontends mutable VM access. Similar read-only views distinguish native and
user-defined dictionary entries and expose user-word bodies. These views reuse
state the VM already retains; normal instruction dispatch does no extra capture
or dictionary bookkeeping for the debugger.

The first frontend is the terminal debugger, tdb, available from the REPL and
through `--tdb` for files and evaluated source. A machine frontend uses the
same hook for `toy-dap`. Its private record-separated stream multiplexes paused
snapshots with ordinary Toy output; the Go adapter translates those records to
DAP without parsing tdb's human-oriented text. Both frontends use the internal
debug controller for step-in/over/out state and source or word breakpoint
filtering, while retaining their own command parsing and presentation.

Unhandled diagnostics inspect the same live VM state before error unwinding.
They print a bounded data-stack snapshot and, when execution is nested, a
bounded call chain of Toy program frames. Native continuation frames remain an
implementation detail in these user-facing reports. Error suppression used by
`try` prevents handled failures from producing diagnostics.

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
- resource objects run their external destructor before their released object
  storage enters that cache;
- many update-style words mutate only when `refcount == 1`, otherwise they
  clone first to preserve value semantics.
- vector `filter` results keep up to two matches inline, then reserve at most
  64 slots based on input length to avoid repeated geometric growth without
  retaining unbounded excess capacity for sparse results.

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

### Strings, Symbols, and Calls

Strings, symbols, and call instructions all store byte sequences. Short values
fit inside the object itself; longer values use one heap allocation. Exact-size
producers allocate the final storage once, while dynamic string producers grow
geometrically and transfer their buffer into the final object.

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
.\nob.exe --mode alloc build
.\benchmarks\run.ps1 -Toy .\build\clang\alloc\toy.exe
```

The `alloc` mode reports checked allocation calls and cumulative
requested bytes. These totals compare identical workloads; they are not live or
peak memory. Timing and allocation workloads live in `benchmarks/`, including
`runtime-internals.toy` for continuations, captures, predicate snapshots, and
recursion schemes.
