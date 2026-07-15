# SQLite Interop Example

This directory uses SQLite as a realistic test of Toy's general native-module
boundary. SQLite is not a Toy dependency or built-in integration. The C file is
a handwritten adapter built with the same generic `module` command available
to any project.

The adapter is currently handwritten because SQLite relies on opaque
`sqlite3 *` and `sqlite3_stmt *` handles, output parameters, borrowed result
buffers, and library-specific cleanup. Toy's manifest generator does not yet
describe those concepts. The example therefore documents the useful target for
the generator rather than adding a special SQLite path to Toy.

After installing SQLite's development files, provide its include path and
library. For example:

```powershell
.\nob.exe module sqlite examples\interop\sqlite\toy_sqlite.c `
    --include C:\sqlite\include `
    --lib-dir C:\sqlite\lib `
    --lib sqlite3
```

You can also pass a library file directly. Using a static SQLite archive makes
the resulting Toy module self-contained; using an import/shared library means
SQLite's runtime library must also be discoverable by the operating system.

Point Toy at the generated module and run the example:

```powershell
$env:TOY_MODULE_PATH = (Resolve-Path .\build\clang\release\modules).Path
.\nob.exe run examples\interop\sqlite\people.toy
```

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
