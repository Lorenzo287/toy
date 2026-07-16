# Generated Binding Examples

`clib.json` generates direct C wrappers for `strlen` and `strcmp`, compiles them
as a normal native module, and calls the resulting Toy words:

```powershell
.\nob.exe bindgen clib examples\interop\bindgen\clib.json
$env:TOY_MODULE_PATH = (Resolve-Path .\build\clang\release\modules).Path
.\nob.exe run examples\interop\bindgen\demo.toy
```

`stdio.json` wraps `FILE *` as an owned
`stdio.generated.file` resource. Dropping the final Toy reference calls
`fclose` automatically:

```powershell
.\nob.exe bindgen stdio.generated examples\interop\bindgen\stdio.json
$env:TOY_MODULE_PATH = (Resolve-Path .\build\clang\release\modules).Path
.\nob.exe run examples\interop\bindgen\stdio-demo.toy
```

These standard-C functions need no additional library configuration. See
[`docs/bindgen.md`](../../../docs/bindgen.md) for the manifest contract.

`sqlite.json` is a generated third-party binding rather than a Toy-provided
SQLite integration. It demonstrates hidden C arguments, resource-specific
errors, binary-safe pointer-length strings, and a statement that retains its
database:

```powershell
.\nob.exe bindgen sqlite.generated examples\interop\bindgen\sqlite.json `
    --include C:\sqlite\include `
    --lib-dir C:\sqlite\lib `
    --lib sqlite3
$env:TOY_MODULE_PATH = (Resolve-Path .\build\clang\release\modules).Path
.\nob.exe run examples\interop\bindgen\sqlite-demo.toy
```

The selected compiler must match the SQLite library's ABI. The separate
[`examples/interop/sqlite/`](../sqlite/) adapter remains handwritten to show
custom row-state validation and a more idiomatic Toy interface.
