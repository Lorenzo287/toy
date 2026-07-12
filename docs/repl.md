# Toy REPL

Running `toy` without a file starts an interactive REPL. The REPL is the best
place to learn Toy because the language can document itself: inspect the word
catalog, ask for a word's docs, try a small expression, and immediately see the
stack.

```powershell
cmake --build build
.\build\toy.exe
```

## Interactive Discovery

Useful first commands:

```text
toy> help
toy> 'map doc print
toy> 'map see print
toy> "priority" search-words
toy> words
```

The catalog groups words by primary concept, not by implementation file. Shared
words such as `push-back` or `pairs` are listed under their main language idea
even when they support more than one representation.

## Interactive Development Loop

Type `trace` to toggle the automatic stack display. Type `hints` to toggle
syntax-aware input hints.

Definitions persist for the whole REPL session.

The data stack also persists. By default, the REPL prints the stack after each
evaluation. Use `.s` or `.S` when you want to inspect the stack explicitly 
(might be useful when trace toggle is off), `empty` to clear it.

## Scope of Captures

Captures created with `| ... |` live only while the word or quotation that
created them is running. They do not become persistent REPL variables:

```text
toy> 5 |a| $a print
5
toy> 5 |a|
toy> $a print
runtime error: undefined variable '$a'
```

Use `def` for persistent names.

## Multiline Input

The REPL keeps reading until the current input is structurally complete.

This applies to:

- quotations/vectors: `[ ... ]`
- lists: `( ... )`
- maps/sets: `{ ... }` / `#{ ... }`
- capture lists: `| ... |`
- strings: `" ... "`
- block comments: `/* ... */`

## Editing, History, Completion

The REPL uses `linenoise` for:

- line editing;
- command history;
- tab completion for known words;
- syntax-aware hints (`hints`);
- automatic stack display (`trace`);
- categorized help (`help`).

Stack display uses unambiguous value forms. Deques and priority queues are
shown as `deque[...]` and `pqueue[...]` display forms so they read as single
values. `repr` and `.S` still use reconstructable source forms.

## Exiting and Interrupting

To exit:

- Unix-like systems / WSL: `Ctrl-D`
- Windows console: `Ctrl-Z`
- at the prompt: press `Ctrl-C` twice in a row
- from Toy code: `exit`

Use `Ctrl-C` once to interrupt a running program such as an infinite loop. The
REPL reports this as an interrupt rather than as a generic runtime error. When
no program is running, the first `Ctrl-C` clears the current input and prints a
reminder that pressing `Ctrl-C` again exits.
