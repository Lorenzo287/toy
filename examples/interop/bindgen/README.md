# Generated Binding Example

This example generates direct C wrappers for `strlen` and `strcmp` from an
explicit manifest, compiles them as a normal native module, and calls the
resulting Toy words:

```powershell
.\nob.exe bindgen clib examples\interop\bindgen\clib.json
$env:TOY_MODULE_PATH = (Resolve-Path .\build\clang\release\modules).Path
.\nob.exe run examples\interop\bindgen\demo.toy
```

The standard-C functions need no additional library configuration. See
[`docs/bindgen.md`](../../../docs/bindgen.md) for the manifest contract.
