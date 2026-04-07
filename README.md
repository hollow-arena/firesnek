> **Status: v0-alpha — active development, not production-ready.** The transpiler is incomplete and the runtime API is subject to change. This repo is public for visibility, not as a stable release.

# firesnek

Compile your existing Python to C. No type annotations. No CPython runtime. 60x faster.

```python
def n_queens(n):
    board = [-1] * n
    return solve(board, 0, n)

print(n_queens(13))  # 73712
```

```bash
pip install firesnek
firesnek your_file.py
gcc out.c -o out && ./out
```

**Fibonacci(30), 2500 iterations:**

|          | Total time  | Per call |
|----------|-------------|----------|
| Python   | 152,443 ms  | 60.9 ms  |
| firesnek | 2,520 ms    | 1.0 ms   |

**60x faster. Real Python syntax. No GC. No runtime.**

*`gcc -std=c11 -g` (no optimization). Early development build — numbers will improve.*

---

## What is firesnek?

firesnek is a Python-to-C transpiler. Point it at a `.py` file and get a `.c` file back — compilable with any C toolchain, no CPython, no VM, no interpreter.

The runtime uses NaN-boxed duck typing — every value is a single 64-bit word, no boxing overhead, no garbage collector. You write normal Python. firesnek handles the rest.

---

## How it works

- **Parses real Python** — uses Python's built-in `ast` module. No custom lexer or parser.
- **Emits C** — transpiles to a flat `.c` file compilable with `gcc` or `clang`.
- **Duck-typed runtime** — all values are NaN-boxed 64-bit words. Integers, floats, booleans, and pointers all fit in a single `uint64_t`.
- **Arena memory** — allocations are freed deterministically at scope exit. No GC pauses.

---

## Try it

> **Not yet packaged.** To experiment with the current alpha, clone the repo and invoke the compiler directly:

```bash
python src/compiler/test_compile.py
gcc tests/test_compile.c -o out && ./out
```

---

## Roadmap

**v0-alpha (in progress)**
- [x] Python AST parsing (via `ast` module)
- [x] Duck / NaN-boxing runtime
- [x] Arena allocator (single global arena)
- [ ] C emission — function defs, classes, control flow, expressions
- [ ] Basic built-ins (print, range, len)

**v1**
- [ ] Per-scope arena frames
- [ ] Return value promotion
- [ ] `pip install firesnek` packaging + CLI
- [ ] Standard library coverage
- [ ] @export decorator — compile any Python function to a .so callable from Python, C, Rust, or anything else

---

## License

Apache 2.0 — Copyright 2026 Zack Perez