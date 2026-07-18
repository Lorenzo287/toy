# Dynamic FFI Core Package

`toy_ffi.c` implements the official `core:ffi` package. It uses libffi to call
fixed scalar and C-string signatures selected at runtime and consumes Toy's
public shared-package API rather than becoming part of the VM.

The normal Toy build compiles and installs it beside the CLI. See
[`docs/ffi.md`](../../docs/ffi.md) for build dependencies, signatures, and
safety constraints. A runnable package lives under
[`examples/interop/ffi/`](../../examples/interop/ffi/).
