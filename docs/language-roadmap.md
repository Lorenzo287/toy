# Toy Language Roadmap

This roadmap records Toy's current development status and larger active or
future tracks. It is not a language reference, an agent manual, or a changelog.

## Status Labels

- **In progress**: an active or ongoing development track.
- **Future**: an accepted direction that has not started or is intentionally
  deferred.

## Current Baseline

Toy already has the language, collection, tooling, testing, formatting, and
debugging foundations needed by the tracks below. Current behavior belongs in
the [README](../README.md) and focused references; completed work belongs in Git
history rather than this roadmap.

## Work in Progress

### Performance Work

**Status: In progress**

Keep optimization work benchmark-driven and record durable experiments under
`benchmarks/results/`. Current investigation candidates are:

- dictionary lookup and word dispatch;
- allocation and object-lifetime hot spots;
- call-frame specialization;
- cache behavior for list, vector, and string workloads;
- structural hashes if map/set key policy expands.

### C Interop

**Status: In progress**

Embedding, C extensions, `core:ffi`, and generated bindings share one
value and ownership model. They are usable parts of Toy; this track concerns
how far the boundary grows and which parts need an explicit compatibility
policy.

Priorities are:

1. define the compatibility policy for the public C API and C-extension ABI;
2. extend generated bindings to general output buffers and selected aggregate
   types once their ownership rules are clear;
3. add a libclang frontend that produces the same explicit manifest rather
   than trying to infer ownership from C declarations;
4. design callbacks and native calls into Toy without re-entering the VM or
   losing its iterative execution model.

Release SDKs should continue to carry everything needed for this boundary:
public headers, the embedding archive, core packages, package tools, examples,
and focused reference docs.

## Future Work

### Compiler Backends

**Status: Future**

Treat compilation as a learning track with explicit semantic checkpoints:

1. define an IR for quotations, calls, stack effects, captures, and source
   spans;
2. compile a constrained subset to bytecode for the current VM, retaining an
   interpreter fallback for dynamic behavior;
3. define the runtime ABI used by compiled code for values, errors, dictionary
   lookup, and native calls;
4. experiment with the compact [QBE compiler backend](https://c9x.me/compile/)
   after bytecode semantics are stable;
5. consider LLVM if Toy later needs broader target support or a more ambitious
   optimization pipeline.

Compilation must preserve runtime `def`, first-class quotations and symbols,
dynamic captures, predicate stack sandboxing, error behavior, and value
ownership. Early native compilation should target an explicit subset rather
than silently changing those semantics.
