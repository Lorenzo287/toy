# Basic C Package Example

This is the smallest complete C-backed Toy package. It has no foreign-library
dependency: `toy_basic.c` exports one `twice` word.

Copy the example from the SDK into an editable project directory before
building it:

```powershell
$ToySdk = 'C:\Tools\Toy'
Copy-Item "$ToySdk\examples\packages\basic" .\basic -Recurse
cd .\basic
```

## Manual Build

Compile the C file as a shared library. It includes only the SDK's standalone
`toy_package.h`; do not link `toy_runtime`:

```powershell
clang -std=c11 -Wall -Wextra -Wpedantic -shared `
    .\toy_basic.c -I "$ToySdk\include" `
    -o .\toy_basic.dll
@'
name = basic
native = toy_basic.dll
'@ | Set-Content -NoNewline .\toy.package

toy --file .\demos\basic.toy .
```

On Linux, add `-fPIC` and write `toy_basic.so` in both places. On macOS use
`-dynamiclib` and `toy_basic.dylib`.

## Optional Helper

`toy-c-package` performs the same compile, link, and manifest steps, printing
the compiler commands it runs:

```powershell
toy-c-package . .\toy_basic.c
toy --file .\demos\basic.toy .
```
