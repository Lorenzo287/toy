# Toy Examples

These programs demonstrate Toy for readers and experimentation. They are kept
separate from the automated regressions under `toy/tests/` and favor readable,
standalone source over test narration.

- [`csv.toy`](csv.toy): file and string processing through a small CSV example;
- [`factorial.toy`](factorial.toy): direct recursion and recursion-scheme variants;
- [`mergesort.toy`](mergesort.toy): merge sort built with `binrec` and `merge`;
- [`qsort.toy`](qsort.toy): quicksort built from quotations and `binrec`;
- [`quines/`](quines/): self-reproducing programs using formatting, `repr`, and
  `see`.

Run an example from the repository root:

```powershell
.\build\toy.exe toy\examples\factorial.toy
```

The quine directory disables automatic formatting because whitespace and
embedded source text are part of those examples.
