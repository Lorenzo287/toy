# SQLite Interop Example

This directory uses SQLite as a realistic test of Toy's general native-module
boundary. SQLite is not a Toy dependency or built-in integration. The C file is
a handwritten adapter built with the same generic `module` command available
to any project.

The adapter is handwritten because it adds row-state validation, tailored
errors, and an idiomatic interface beyond direct C calls. The focused
[`sqlite.json`](../bindgen/sqlite.json) manifest demonstrates that the generic
generator can cover SQLite's opaque handles, output parameters, dependent
lifetimes, hidden arguments, and borrowed result buffers without a special
SQLite path in Toy.

After installing a SQLite build compatible with your compiler, compile the
adapter directly. A typical Windows Clang command run from the repository root
is:

```powershell
clang -std=c11 -shared -Iinclude -IC:\sqlite\include `
    examples\interop\sqlite\toy_sqlite.c C:\sqlite\lib\sqlite3.lib `
    -o toy_sqlite.dll
```

The equivalent generic Nob command is:

```powershell
.\nob.exe module sqlite examples\interop\sqlite\toy_sqlite.c `
    --include C:\sqlite\include --lib C:\sqlite\lib\sqlite3.lib
```

Using a static SQLite archive makes the resulting Toy module self-contained;
using an import/shared library means SQLite's runtime library must also be
discoverable by the operating system.

Point Toy at the generated module and run the example:

```powershell
$env:TOY_MODULE_PATH = (Resolve-Path .).Path
.\nob.exe run examples\interop\sqlite\people.toy
```

When using `nob module`, point `TOY_MODULE_PATH` at the matching
`build\<compiler>\<mode>\modules` directory instead.

`open` and `prepare` return typed Toy resources. The database uses
`sqlite3_close_v2` and statements use `sqlite3_finalize`, so `drop`, collection
release, errors, and state shutdown all clean them up automatically. Native
words consume their inputs like ordinary Toy words; use `dup` when a database
or statement is needed again.

The example exposes a deliberately small but useful slice:

- databases: `open`, `exec`, `changes`, and `last-insert-rowid`;
- statements: `prepare`, `step`, `reset`, and `clear-bindings`;
- parameters: `bind-int`, `bind-float`, `bind-text`, and `bind-null`;
- rows: `column-count`, `column-name`, `column-null?`, `column-int`,
  `column-float`, and `column-text`.

It is an interoperability example, not a promise that Toy will maintain a
complete SQLite wrapper.
