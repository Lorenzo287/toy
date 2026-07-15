# Generated C Bindings

`tools/generate-binding.js` turns an explicit JSON manifest into a loadable Toy
native module written in C. Generated modules expose ordinary qualified words;
their users do not interact with `ffi.open`, signatures, or function resources.

The binding generator deliberately reads declared
function shapes rather than parsing arbitrary C headers. That keeps type and
ownership decisions visible while the supported boundary is still small.

## Manifest

The manifest names the Toy module, the C headers used to compile its wrappers,
and the exported functions:

```json
{
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

## Opaque Resources

The optional `resources` array declares owned opaque resources:

```json
{
  "module": "widget",
  "headers": ["widget.h"],
  "resources": [
    {
      "name": "handle",
      "cType": "widget_handle *",
      "destructor": "widget_destroy"
    }
  ],
  "functions": [
    {
      "name": "widget_create",
      "word": "create",
      "returns": {"resource": "handle"},
      "args": ["i32"]
    },
    {
      "name": "widget_value",
      "word": "value",
      "returns": "i32",
      "args": [{"resource": "handle"}]
    },
    {
      "name": "widget_open",
      "word": "open",
      "returns": {"status": "i32", "success": [0]},
      "args": ["cstr", {"outResource": "handle"}]
    }
  ]
}
```

A resource name is local to the module; `handle` above becomes the exact Toy
type `widget.handle`. `cType` must be a C pointer type and `destructor` must be a
C function name. The generator adapts the destructor to Toy's resource
callback, ignoring any C return value.

`{"resource":"handle"}` in `returns` declares a non-null owned pointer
return. The same form in `args` unwraps an exact typed resource and passes the
borrowed C pointer. `{"outResource":"handle"}` contributes no Toy input; it
passes a pointer-to-pointer in its listed C argument position and pushes the
owned handle on success. A function supports at most one output resource and
must return either `void` or a status declaration.

Direct and output resource declarations promise that the returned handle is a
new owned value, not an alias of an input. Dependencies between parent and
child handles are not retained automatically.

A status declaration consumes the C result rather than pushing it. `success`
lists the numeric codes accepted as success; other codes become Toy errors.
Null handles are errors even after a successful status. Handles returned on a
failed status, or rejected by `toy_push_resource()`, are destroyed
automatically. Ordinary resource inputs are consumed like other Toy inputs, so
use `dup` when the same handle is needed again.

Foreign calls happen while borrowed resource inputs remain alive. Returned
`cstr` data is copied before those inputs are released, allowing getters that
borrow text from their handle.

## Build Integration

Generate a C file directly with:

```powershell
node tools\generate-binding.js path\to\clib.json build\generated\clib.c
```

`--check` compares an existing output with the manifest without rewriting it.
Generated files start with a do-not-edit marker and are deterministic.

The Nob `bindgen` command generates the wrapper and builds it as a loadable
module:

```powershell
.\nob.exe bindgen clib path\to\clib.json `
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
.\nob.exe bindgen clib examples\interop\bindgen\clib.json
$env:TOY_MODULE_PATH = (Resolve-Path .\build\clang\release\modules).Path
.\nob.exe run examples\interop\bindgen\demo.toy
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

Declared opaque pointer resources, their destructors, direct owned returns, and
one owned output-resource parameter are supported. Arbitrary raw pointers,
general buffers and output parameters, parent/child handle retention, structs,
unions, variadic functions, callbacks, and library-specific error strings are
not. Status handling currently compares numeric codes, and `cstr` remains the
only borrowed-result policy.

These remaining gaps are why the complete handwritten
[Raylib and SQLite examples](../examples/interop/) have not been replaced by
manifests yet. Header parsing is also not implemented. A later libclang
frontend can produce the same validated manifest model, but ownership and
lifetime policy must remain explicit because C declarations cannot infer it.
