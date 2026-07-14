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

### Modules and Namespaces

**Status: In progress**

Source modules now provide load-once `require`, private definitions, explicit
exports, relative dependency loading, module-scoped aliases, and `.` qualified
names. Keep the model small while real programs establish whether configurable
search paths or additional module metadata are worthwhile. Native modules
should reuse this namespace and load-state machinery rather than introduce a
parallel system.

### Performance Work

**Status: In progress**

Keep optimization work benchmark-driven and record durable experiments under
`benchmarks/results/`. Current investigation candidates are:

- dictionary lookup and word dispatch;
- allocation and object-lifetime hot spots;
- call-frame specialization;
- cache behavior for list, vector, and string workloads;
- structural hashes if map/set key policy expands.

### Runtime API and Foreign Function Interface

**Status: In progress**

Treat the [foreign function interface](https://en.wikipedia.org/wiki/Foreign_function_interface)
as an exploratory interoperability track, not a single general-purpose
`call-c` word. The CLI now links a reusable static runtime, and experimental API
version zero exposes opaque states, primitive and typed resource stack access,
host-to-Toy calls, and synchronous native-word or native-module registration.
Native module descriptors reuse source-module names, exports, load state, and
aliases. The API is not stable yet. A small optional Raylib module now
exercises the boundary with a Toy-owned window and drawing loop plus
automatically unloaded texture resources.

Current follow-up candidates are:

1. extend the resource boundary to sounds and similar values when bindings need
   them, preserving explicit type and lifetime rules;
2. define a versioned shared-library ABI and loader if separately distributed
   native modules become useful;
3. use [libffi](https://github.com/libffi/libffi) to experiment with explicit
   scalar and string signatures resolved at runtime;
4. consider foreign pointers, output parameters, structs, variadic functions,
   and callbacks only after the basic ownership and VM-boundary rules are
   settled.

The resulting boundary should be reusable by embedders, native modules, and
compiled Toy code. Native calls that schedule Toy code must preserve the
iterative VM execution model.

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
