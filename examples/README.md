# Toy Examples

Examples are grouped by the language used to host or express them:

- [`toy/`](toy/README.md) contains standalone Toy programs;
- [`c/embed.c`](c/embed.c) embeds the runtime, exposes a native C word to Toy,
  then calls a Toy word from C.

Build and run the C embedding example from the repository root:

```powershell
cmake --build build --target toy_embed_example
.\build\toy_embed_example.exe
```
