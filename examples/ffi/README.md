# Dynamic FFI Example

This package imports `core:ffi`, opens the C runtime passed as its first
argument, and calls `strlen`. Run it directly from the SDK without changing the
SDK:

```powershell
$ToySdk = 'C:\Tools\Toy'
toy --file "$ToySdk\examples\ffi\main.toy" msvcrt.dll
```

Use the appropriate C runtime library name on other platforms. See
[`docs/ffi.md`](../../docs/ffi.md) for source-build dependencies, supported
signatures, and safety constraints.
