# Generated C Bindings

`tools/generate-binding.js` turns an explicit JSON manifest into an importable
Toy native package written in C. Generated packages expose ordinary qualified words;
their users do not interact with `ffi.open`, signatures, or function resources.

The binding generator deliberately reads declared function shapes rather than
parsing arbitrary C headers. That keeps type and ownership decisions visible
while the supported boundary is still small.

## Manifest

The manifest names the Toy package, the C headers used to compile its wrappers,
and the exported functions:

```json
{
  "package": "clib",
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

`name` is the C identifier. Optional `word` changes the public Toy word name;
otherwise the C name is used. Package and word names follow the normal native
package rules. Duplicate words, unsafe header paths, unknown fields, unsupported
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

## Hidden and Length-Aware Arguments

Argument objects can provide C-only values that do not consume the Toy stack:

```json
"args": [
  {"resource": "database"},
  "cstr",
  {"const": "i32", "value": -1},
  {"outResource": "statement"},
  {"null": true}
]
```

`const` accepts a supported boolean, integer, or floating type and a matching
JSON value. `null` emits `NULL`. `cConstant` emits one validated C identifier,
such as `{"cConstant":"SQLITE_TRANSIENT"}`, whose declaration must come from
one of the manifest headers. These forms are deliberately expressions rather
than Toy inputs.

A byte argument expands one binary-safe Toy string into adjacent pointer and
length C arguments:

```json
{"bytes": {"length": "i32"}}
```

The generator passes the string's borrowed bytes directly and range-checks its
length for the declared integer type. The C function must copy the bytes if it
keeps them after returning.

A borrowed pointer result can use a companion C function to obtain its length:

```json
"returns": {
  "bytes": {
    "lengthFunction": "widget_result_size",
    "lengthType": "usize"
  }
}
```

The length function receives the same C arguments as the primary function. The
generator validates the returned length and copies the bytes into a Toy string
before consuming resource or string inputs. A null pointer or invalid length is
an error.

## Opaque Resources

The optional `resources` array declares owned opaque resources:

```json
{
  "package": "widget",
  "headers": ["widget.h"],
  "resources": [
    {
      "name": "handle",
      "cType": "widget_handle *",
      "destructor": "widget_destroy"
    },
    {
      "name": "view",
      "cType": "widget_view *",
      "destructor": "widget_view_destroy",
      "dependsOn": "handle"
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
      "name": "widget_view",
      "word": "view",
      "returns": {"resource": "view"},
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

A resource name is local to the package; `handle` above becomes the exact Toy
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
new owned value, not an alias of an input. A resource with `dependsOn` keeps one
parent resource alive until the child destructor finishes. Every function that
produces that child must have exactly one resource input of the named parent
type, avoiding ambiguous lifetime choices.

A status declaration consumes the C result rather than pushing it. `success`
lists the numeric codes accepted as success; other codes become Toy errors.
For predicate-like APIs, `map` replaces `success` and maps accepted numeric
codes to Toy booleans:

```json
"returns": {"status": "i32", "map": {"100": true, "101": false}}
```

An optional error accessor adds a borrowed library message:

```json
"returns": {
  "status": "i32",
  "success": [0],
  "error": {"function": "widget_error", "resource": "handle"}
}
```

The selected resource must occur exactly once among the function's resource
inputs or outputs. The accessor is called before inputs and failed output
handles are released; its returned C string is copied into Toy's diagnostic.
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
`--package name` additionally verifies the manifest package when another build
system owns the output directory.
Generated files start with a do-not-edit marker and are deterministic.
They instantiate the standalone `toy_package.h` implementation, so the result
can be compiled with an ordinary C compiler and the target C library alone:

```powershell
clang -std=c11 -shared -I include build\generated\clib.c `
    -o toy_clib.dll -lthe_c_library
```

No Toy runtime or package-support library participates in that link. On Linux,
add `-fPIC` and use the `.so` suffix.

The Nob `bindgen` command generates the wrapper, builds it, and writes a
`toy.package` manifest in one step. It is a convenience around the same
generator and compiler invocation:

```powershell
.\nob.exe bindgen vendor\clib path\to\clib.json `
    --include path\to\headers `
    --lib the_c_library
```

The first argument is the package directory. Its basename determines the
output filename (`toy_clib` plus the platform shared-library suffix) and must
match the manifest's `package` value.
Headers remain in the manifest because they affect generated C; repeatable
`--include`, `--lib-dir`, and `--lib` options provide platform-specific
dependency locations without putting them in the manifest.

The curated standard-C example needs no additional library configuration:

```powershell
.\nob.exe bindgen examples\interop\bindgen\clib `
    examples\interop\bindgen\clib\clib.json
.\nob.exe run examples\interop\bindgen\demos\clib
```

Toy sees a normal package:

```toy
"../../clib" 'c import-as
"Toy speaks generated C" c.strlen print
"alpha" "beta" c.strcmp 0 < print
```

## Current Boundary

Generated wrappers call the declared C functions directly. This lets the C
compiler apply declarations and the target calling convention, but the
manifest must still match the header semantically. Compatible C conversions can
hide a mistaken scalar declaration.

Declared opaque pointer resources, dependent lifetimes, direct owned returns,
one owned output-resource parameter, hidden constants and nulls, resource-based
error accessors, boolean result-code mappings, and adjacent pointer-length
strings are supported. Arbitrary raw pointers, general output buffers, structs,
unions, variadic functions, callbacks, and stateful library-specific
translation are not.

The generated SQLite example exercises the general policies above. The larger
handwritten Raylib and SQLite adapters remain useful where calls need custom
state or validation. Header parsing is not implemented. A later libclang
frontend can produce the same validated manifest model, but ownership and
lifetime policy must remain explicit because C declarations cannot infer it.
