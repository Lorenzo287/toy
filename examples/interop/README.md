# External-Library Interop Examples

These examples test how a Toy project can reuse C libraries without adding
those libraries to the Toy runtime. Every resulting module is found through
`TOY_MODULE_PATH` and loaded by the normal Toy executable with `require`.

- [Dynamic FFI](./ffi/) resolves scalar and C-string functions at runtime.
- [Generated bindings](./bindgen/) compile an explicit manifest into direct C
  wrappers and ordinary Toy words.
- [Raylib](./raylib/) exercises a stateful graphics API and owned texture
  values.
- [SQLite](./sqlite/) exercises opaque database and statement handles,
  prepared parameters, borrowed row data, and deterministic cleanup.

Raylib and SQLite are deliberately examples rather than Toy-maintained library
packages. They have no dedicated host executable, build command, or core
runtime dependency.

Today, projects have three increasingly flexible choices:

1. use `bindgen` for declared scalar and C-string functions or basic owned
   opaque resources;
2. use the optional `ffi` module for dynamic scalar and C-string calls;
3. write a small C module when the library requires buffers, aggregates,
   callbacks, dependent resources, or other library-specific translation.

The generator should continue making the third choice smaller without adding
library-specific logic to Toy. SQLite remains the concrete test for status
mapping, hidden constant arguments, and length-aware borrowed buffers.
