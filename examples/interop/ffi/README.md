# Dynamic FFI Example

This package imports `core:ffi`, opens the C runtime passed as its first
argument, and calls `strlen`. The normal Toy build already compiles the FFI
package:

```powershell
toy examples\interop\ffi\strlen msvcrt.dll
```

Use the appropriate C runtime library name on other platforms. See
[`docs/ffi.md`](../../../docs/ffi.md) for source-build dependencies, supported
signatures, and safety constraints.
