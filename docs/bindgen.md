# Generated C Bindings

`tools/generate-binding.js` turns an explicit JSON manifest into a loadable Toy
native module written in C. Generated modules expose ordinary qualified words;
their users do not interact with `ffi.open`, signatures, or function resources.

This is the first binding-generator contract. It deliberately reads declared
function shapes rather than parsing arbitrary C headers. That keeps type and
ownership decisions visible while the supported boundary is still small.

## Manifest

The manifest names the Toy module, the C headers used to compile its wrappers,
and the exported functions:

```json
{
  "schemaVersion": 1,
  "module": "clib",
  "headers": ["string.h"],
  "functions": [
    {
      "name": "strlen",
      "returns": "usize",
      "args": ["cstr"]
    },
    {
      "name": "strcmp",
      "word": "compare",
      "returns": "i32",
      "args": ["cstr", "cstr"]
    }
  ]
}
```

`name` is the C identifier. Optional `word` changes the local Toy export name;
otherwise the C name is used. Module and word names follow the normal native
module rules. Duplicate words, unsafe header paths, unknown fields, unsupported
types, and `void` arguments are rejected before C is emitted.

The supported types match the scalar/string FFI experiment:

- `bool`;
- `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, and `u64`;
- `isize` and `usize`;
- `f32` and `f64`;
- `cstr`;
- `void` as a return type only.

Integer inputs are range-checked. Toy strings are copied to temporary
NUL-terminated buffers that remain valid only for the duration of the C call.
A non-null returned `cstr` is treated as borrowed and copied into Toy before the
wrapper returns. Unsigned results outside Toy's signed 64-bit range are errors.

## Build Integration

Generate a C file directly with:

```powershell
node tools\generate-binding.js bindings\clib.json build\generated\clib.c
```

`--check` compares an existing output with the manifest without rewriting it.
Generated files start with a do-not-edit marker and are deterministic.

The Nob `bindgen` command generates the wrapper and builds it as a loadable
module:

```powershell
.\nob.exe bindgen clib bindings\clib.json `
    --include path\to\headers `
    --lib the_c_library
```

The first argument determines the output filename (`toy_clib` plus the platform
shared-library suffix). It should match the module name used by `require`.
Headers remain in the manifest because they affect generated C; repeatable
`--include`, `--lib-dir`, and `--lib` options provide platform-specific
dependency locations without putting them in the manifest.

The curated standard-C example needs no additional library configuration:

```powershell
.\nob.exe bindgen clib examples\bindings\clib.json
$env:TOY_MODULE_PATH = (Resolve-Path .\build\clang\release\modules).Path
.\nob.exe run examples\toy\generated_clib.toy
```

Toy sees a normal module:

```toy
"clib" 'c require-as
"Toy speaks generated C" c.strlen print
"alpha" "beta" c.strcmp 0 < print
```

## Current Boundary

Generated wrappers call the declared C functions directly. This lets the C
compiler apply declarations and the target calling convention, but the
manifest must still match the header semantically. Compatible C conversions can
hide a mistaken scalar declaration.

Raw pointers, buffers, output parameters, owned returns, structs, unions,
variadic functions, and callbacks are not supported. Header parsing is also not
implemented yet. A later libclang frontend can produce this same validated
manifest model rather than changing the generated module ABI.
