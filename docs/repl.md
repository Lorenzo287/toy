# Toy REPL

Toy includes an interactive REPL when started without a filename.

## Starting It

```powershell
cmake --build build
.\build\toy.exe
```

On Unix-like systems or WSL:

```bash
cmake --build build
./build/toy
```

## What Persists Between Entries

The REPL keeps one interpreter context alive across entries. This means:

- user-defined words persist
- the data stack persists
- the global dictionary persists

Example:

```forth
: sq {n} $n $n * ;
5 sq print
```

## What Does Not Persist

Variable captures created with `{ ... }` are local to the current execution,
not to the whole REPL session.

This works:

```forth
5 {a} $a print
```

This does not:

```forth
5 {a}
$a print
```

The reason is that captured variables live in execution frames and are released
when that top-level REPL command finishes.

## Multiline Input

The REPL keeps reading until the current input is structurally complete.

This applies to:

- blocks: `[ ... ]`
- variable lists: `{ ... }`
- strings: `" ... "`
- colon definitions: `: ... ;`

Example:

```forth
: sq
{n}
$n $n *
;
```

## Editing, History, and Completion

The REPL uses vendored `linenoise`:

- in-line editing
- command history
- tab completion for known words
- syntax-aware hints (type `hints` to toggle)

## Colors and Status

The REPL uses colored output to distinguish the main categories:

- prompt: light blue
- success: green `ok`
- parsing/runtime errors: red labels
- interrupt: yellow `interrupt: ...`
- contextual/fallback messages: dimmer status text

## Exiting

- Unix-like systems / WSL: `Ctrl-D`
- Windows console: `Ctrl-Z`
- Portable explicit exit: `bye`/`exit`

## Interrupting Execution

Use `Ctrl-C` to interrupt a running program, for example an infinite loop.

Interrupts are reported as a dedicated outcome, not as a generic runtime
failure. In the REPL this should produce a single interrupt message rather than
secondary errors from words such as `while`.
