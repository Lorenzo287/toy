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
