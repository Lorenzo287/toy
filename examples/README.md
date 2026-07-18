# Examples

Examples are grouped by what they demonstrate rather than by implementation
language:

- [`programs/`](./programs/) contains executable Toy package directories and
  algorithms;
- [`eval/`](./eval/) contains raw evaluation examples such as
  formatting-sensitive quines;
- [`embedding/`](./embedding/) contains C applications that embed the Toy
  runtime;
- [`interop/`](./interop/) contains dynamic, generated, and handwritten ways
  for Toy programs to call external C code.

Correctness regressions belong under `tests/`; these files are intended to be
read and run by users.

With an installed SDK, run an executable package directly:

```powershell
toy examples\programs\factorial
```

Interop examples use the installed `toy-c-package` and `toy-bindgen` tools. Their
individual READMEs include the required compiler and foreign-library options.
