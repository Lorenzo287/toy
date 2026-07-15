# Optional Modules

This directory contains optional, general-purpose native modules maintained
with Toy. They are not linked into the core runtime and are loaded explicitly
with `require`.

Library-specific demonstrations such as Raylib and SQLite belong under
[`examples/interop/`](../examples/interop/) instead of here.

- [`ffi/`](./ffi/) provides the experimental dynamic libffi module.
