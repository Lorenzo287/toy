# External-Library Interop Examples

These examples show how a Toy project can reuse C libraries without adding
them to the runtime. Generated and handwritten wrappers are ordinary package
directories imported by exact path; there is no search-path environment
variable or package manager.

- [Dynamic FFI](./ffi/) imports the official `core:ffi` package and resolves
  scalar and C-string functions at runtime.
- [Generated bindings](./bindgen/) compile explicit manifests into direct C
  wrappers, including a focused SQLite binding.
- [Raylib](./raylib/) exercises a stateful graphics API and owned texture
  values through a handwritten package.
- [SQLite](./sqlite/) exercises opaque database and statement handles,
  prepared parameters, borrowed row data, and deterministic cleanup.

Raylib and SQLite are deliberately examples rather than Toy-maintained library
packages. They have no custom host executable or runtime integration.
Handwritten adapters use the standalone `toy_package.h` interface; Nob's
`package` command compiles the wrapper and writes its `toy.package` manifest.

The practical choices are:

1. use `core:ffi` for dynamic scalar and C-string calls;
2. use `bindgen` for explicitly declared functions and resource ownership;
3. write a small C package for structs, callbacks, general buffers, or custom
   state and lifetime translation.

The generator should continue making the third choice smaller without adding
library-specific logic to Toy.
