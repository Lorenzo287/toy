# External-Library Interop Examples

These examples test how a Toy project can reuse C libraries without adding
those libraries to the Toy runtime. Every resulting module is found through
`TOY_MODULE_PATH` and loaded by the normal Toy executable with `require`.

- [Dynamic FFI](./ffi/) resolves scalar and C-string functions at runtime.
- [Generated bindings](./bindgen/) compile explicit manifests into direct C
  wrappers and ordinary Toy words, including a focused SQLite binding.
- [Raylib](./raylib/) exercises a stateful graphics API and owned texture
  values.
- [SQLite](./sqlite/) exercises opaque database and statement handles,
  prepared parameters, borrowed row data, and deterministic cleanup.

Raylib and SQLite are deliberately examples rather than Toy-maintained library
packages. They have no dedicated host executable, build command, or core
runtime dependency.

Today, projects have three increasingly flexible choices:

1. use `bindgen` for declared scalar/string functions, owned and dependent
   opaque resources, hidden C arguments, and pointer-length strings;
2. use the optional `ffi` module for dynamic scalar and C-string calls;
3. write a small C module when the library requires general output buffers,
   aggregates, callbacks, or other stateful library-specific translation.

The generator should continue making the third choice smaller without adding
library-specific logic to Toy. The generated and handwritten SQLite examples
show where the declarative boundary ends and custom adapter behavior begins.
