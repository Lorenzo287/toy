# SQLite Interop Example

This directory uses SQLite as a realistic test of Toy's native-package
boundary. SQLite is not a Toy dependency or core integration.

The adapter is handwritten because it adds row-state validation, tailored
errors, and an idiomatic interface beyond direct C calls. The focused
[`sqlite.json`](../bindgen/sqlite/sqlite.json) manifest shows how far the
generic generator can cover the same C API.

After installing a SQLite build compatible with your compiler, build the
package in place:

```powershell
.\nob.exe package examples\interop\sqlite `
    examples\interop\sqlite\toy_sqlite.c `
    --include C:\sqlite\include `
    --lib C:\sqlite\lib\sqlite3.lib
```

Using a static SQLite archive makes the Toy package self-contained. Using an
import/shared library means SQLite's runtime library must also be discoverable
by the operating system. Run the example package with:

```powershell
.\nob.exe run examples\interop\sqlite\demos\people
```

The demo imports `../..`; no global installation or search path is involved.

`open` and `prepare` return typed Toy resources. The database uses
`sqlite3_close_v2` and statements use `sqlite3_finalize`, so final reference
release and state shutdown clean them up automatically. Native words consume
their inputs like ordinary Toy words; use `dup` when a database or statement is
needed again.

The package exposes a deliberately small slice:

- databases: `open`, `exec`, `changes`, and `last-insert-rowid`;
- statements: `prepare`, `step`, `reset`, and `clear-bindings`;
- parameters: `bind-int`, `bind-float`, `bind-text`, and `bind-null`;
- rows: `column-count`, `column-name`, `column-null?`, `column-int`,
  `column-float`, and `column-text`.

It is an interoperability example, not a promise that Toy will maintain a
complete SQLite wrapper.
