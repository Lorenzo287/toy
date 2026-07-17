# Dynamic FFI Example

This example uses Toy's optional `ffi` module to resolve and call `strlen` at
runtime. Build the module with the libffi development files appropriate for
your compiler:

```powershell
.\nob.exe ffi `
    --include C:\libffi\include `
    --lib-dir C:\libffi\lib
$env:TOY_MODULE_PATH = (Resolve-Path .\build\clang\release\modules).Path
.\nob.exe run examples\interop\ffi\strlen.toy msvcrt.dll
```

Use the appropriate C runtime library name on other platforms. See
[`docs/ffi.md`](../../../docs/ffi.md) for supported signatures and safety
constraints. When using MSYS2/MinGW libffi on Windows, add `--cc gcc` and use
the matching `build\gcc\...` module directory.
