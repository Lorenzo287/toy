# Dynamic FFI Module

`toy_ffi.c` implements the optional native module loaded by `"ffi" require`.
It uses libffi to call fixed scalar and C-string signatures selected at runtime.
The implementation is a consumer of Toy's public shared-module API, not part of
the language or VM.

Build it through the same generic `nob module` command used by other native
modules; see [`docs/ffi.md`](../../docs/ffi.md) for dependency, signature, and
safety details. A runnable program lives under
[`examples/interop/ffi/`](../../examples/interop/ffi/).
