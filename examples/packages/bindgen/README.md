# Generated Binding Examples

Copy the `bindgen/` tree into an editable project directory before following
these commands, then work from that copied directory. `clib/clib.json`
generates direct C wrappers for `strlen` and `strcmp`, builds them into the
`clib` package directory, and calls the resulting Toy words:

```powershell
$ToySdk = 'C:\Tools\Toy'
Copy-Item "$ToySdk\examples\packages\bindgen" .\bindgen -Recurse
cd .\bindgen
```

```powershell
toy-bindgen --package clib `
    .\clib\clib.json `
    .\clib\generated.c
toy-c-package .\clib .\clib\generated.c
toy --file .\demos\clib.toy .\clib
```

`stdio/stdio.json` wraps `FILE *` as an owned `stdio.file` resource. Dropping
the final Toy reference calls `fclose` automatically:

```powershell
toy-bindgen --package stdio `
    .\stdio\stdio.json `
    .\stdio\generated.c
toy-c-package .\stdio .\stdio\generated.c
toy --file .\demos\stdio.toy .\stdio .\demos\stdio.toy
```

These standard-C functions need no additional library configuration. See
[`docs/bindgen.md`](../../../docs/bindgen.md) for the manifest contract.

`sqlite/sqlite.json` is a generated third-party binding rather than a
Toy-provided SQLite integration. It demonstrates hidden C arguments,
resource-specific errors, binary-safe pointer-length strings, and a statement
that retains its database:

```powershell
toy-bindgen --package sqlite `
    .\sqlite\sqlite.json `
    .\sqlite\generated.c
toy-c-package .\sqlite .\sqlite\generated.c `
    --include C:\sqlite\include `
    --lib-dir C:\sqlite\lib `
    --lib sqlite3
toy --file .\demos\sqlite.toy .\sqlite
```

The selected compiler must match the SQLite library's ABI. The separate
[`examples/packages/sqlite/`](../sqlite/) package remains handwritten to show
custom row-state validation and a more idiomatic Toy interface.
