# Generated Binding Examples

`clib/clib.json` generates direct C wrappers for `strlen` and `strcmp`, builds
them into the `clib` package directory, and calls the resulting Toy words:

```powershell
.\nob.exe bindgen examples\interop\bindgen\clib `
    examples\interop\bindgen\clib\clib.json
.\nob.exe run examples\interop\bindgen\demos\clib
```

`stdio/stdio.json` wraps `FILE *` as an owned `stdio.file` resource. Dropping
the final Toy reference calls `fclose` automatically:

```powershell
.\nob.exe bindgen examples\interop\bindgen\stdio `
    examples\interop\bindgen\stdio\stdio.json
.\nob.exe run examples\interop\bindgen\demos\stdio
```

These standard-C functions need no additional library configuration. See
[`docs/bindgen.md`](../../../docs/bindgen.md) for the manifest contract.

`sqlite/sqlite.json` is a generated third-party binding rather than a
Toy-provided SQLite integration. It demonstrates hidden C arguments,
resource-specific errors, binary-safe pointer-length strings, and a statement
that retains its database:

```powershell
.\nob.exe bindgen examples\interop\bindgen\sqlite `
    examples\interop\bindgen\sqlite\sqlite.json `
    --include C:\sqlite\include `
    --lib-dir C:\sqlite\lib `
    --lib sqlite3
.\nob.exe run examples\interop\bindgen\demos\sqlite
```

The selected compiler must match the SQLite library's ABI. The separate
[`examples/interop/sqlite/`](../sqlite/) package remains handwritten to show
custom row-state validation and a more idiomatic Toy interface.
