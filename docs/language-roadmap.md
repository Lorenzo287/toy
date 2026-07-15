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
state-bound persistent values, basic collection construction and traversal,
host-to-Toy value calls, and synchronous native-word or native-module
registration.
Native module descriptors reuse source-module names, exports, load state, and
aliases. Shared native-module ABI version one adds a size-tagged host function
table, a stable entry symbol, platform library discovery through `require`, and
context-owned library handles. The API is not stable yet. A Raylib interop
example exercises the generic shared-module path with a Toy-owned window,
drawing loop, and automatically unloaded texture resources. A SQLite example
exercises opaque database and statement handles, prepared parameters, copied
row data, and automatic finalization. Both remain external-library examples,
not Toy-provided integrations. An optional
libffi module now experiments with dynamically resolved, explicit signatures
for booleans, integers, floats, and copied C strings. An explicit-manifest
generator can compile that same safe subset into ordinary loadable module words;
automatic header parsing is not implemented yet.

Current follow-up candidates are:

1. extend the explicit manifest with typed opaque resources, constructors,
   destructors, output handles, and borrowed/copy result policies, using the
   handwritten SQLite adapter as the concrete target;
2. settle reusable library/package configuration without making the runtime or
   build program aware of individual third-party libraries;
3. add a libclang frontend after the resource manifest is explicit enough to
   preserve ownership decisions that headers cannot infer;
4. expose general buffers, structs, variadic functions, and callbacks only
   after their ownership and VM-boundary rules are settled.

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
