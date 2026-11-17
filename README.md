[English](README.md) | [简体中文](README.zh-CN.md)

# Pace Interpreter (C++17)

Pace is an indentation-sensitive, explicitly typed scripting language with automatic typing (`automatic`). Source files use the `.pace` extension. This repository contains its C++17 interpreter.

> Current version: **v0.4.1**

## Build

```powershell
.\build.ps1            # build (-std=c++17 -Wall -Wextra -pedantic)
.\build.ps1 -Debug     # debug build (-g -O0)
.\build.ps1 -Sanitize  # try ASan (-fsanitize=address); falls back to -Debug when unavailable
```

Output: `pace.exe` in the repository root.

## Run

```powershell
.\pace.exe run <file.pace>
```

Exit codes:

| Exit code | Meaning |
| --- | --- |
| `0` | Finished normally, or the program exited via `exit` |
| `1` | Lexer / parser / parse-time semantic / runtime error (diagnostics to stderr as `file:line:col: 错误: message`) |
| `2` | CLI usage error (missing subcommand, subcommand is not `run`, missing file, extra arguments, file not found) |

Examples:

```powershell
.\pace.exe run examples\hello.pace
.\pace.exe run examples\demo.pace
.\pace.exe run examples\loop_and_exit.pace
```

## Tests

```powershell
.\run_tests.ps1
# or
python tests\run_tests.py
# specify interpreter / filter cases
python tests\run_tests.py --exe .\pace.exe --filter input
```

Cases live under `tests/cases/<name>/`:

| File | Meaning |
| --- | --- |
| `main.pace` | Required; the program under test |
| `input.txt` | Optional; stdin |
| `expected.out` | Optional; exact stdout match (CRLF/LF normalized, BOM ignored) |
| `expected.err_contains` | Optional; each non-empty line must appear as a substring on some stderr line |
| `expected.exit` | Required; expected exit code |
| `expected.warning` | Optional; when present, stderr must contain at least one `: warning: ` |

- stdout is compared exactly; every non-empty line of `expected.err_contains` must appear as a substring on some stderr line (order-independent); each case times out after 10 seconds.
- Error cases (`expected.exit` = 1) additionally require at least one stderr line matching `file:line:col: 错误: `.
- Warning cases require at least one stderr line matching `file:line:col: warning: `, an exact stdout match, and exit code 0.
- Runtime memory-limit warnings use the exact plain-text format `Warning: 变量 '<name>' 超出内存限制 <limit> bit，操作被跳过。` (no `file:line:col` prefix) and are asserted through `expected.err_contains`.
- There are currently **73** cases: 33 positive, 38 error, 2 warning.

## Features

- Indentation-sensitive lexer: space indentation, a consistent indent unit, INDENT/DEDENT, tab errors
- `\note ... \note` multi-line comments (inactive inside strings); bare `\n` newlines within strings
- Types: `integer` / `float` / `boolean` / `string` / `automatic`
- Variable state machine: `initialize` (with an optional `limit <expr>` memory cap) / `reassign` / `free` / `delete` / `automatic`
- Statements: `output` / `input` / `repeat` / `if` / `otherwise` / `exit` / `next` / `return` / `invoke` / `alias`
- Functions: signature pre-scan, forward references, parse-time arity checks, name-conflict checks (parameters / variables / aliases)
- Expression precedence: `and`/`or` (same level, left-associative, short-circuit) → `not` (binds to comparisons) → comparisons (chaining forbidden) → `+ -` → `* /` → unary `-` → primary
- Integer `+ - *` overflow checks; `/` always returns double and errors on division by zero; floating-point `inf` errors
- String concatenation: `+` supports `string + string` (e.g. `"a" + "b"` = `"ab"`); mixing `string` with numbers (`string + integer` / `integer + string`) raises a type-mismatch error; applies equally when an `automatic` variable currently holds a string
- Float-to-integer conversion requires a zero fractional part within int64 range; comparing a string with a number errors; `string → boolean` errors
- `exit N` level semantics (the function body counts as level 1; each enclosing `if`/`repeat` adds one)
- `next` statement: evaluates one or more comma-separated expressions, abandons the rest of the current `repeat` iteration, and passes those values to the next round (a `continue` with a payload); it is only valid inside a `repeat` (including `if`/`otherwise` blocks within it)
- `obtain` expression: `obtain <index>` or `obtain <index> : <default>` reads a 1-based value passed by the previous round's `next` in the nearest `repeat`. The index must evaluate to a positive integer (0, negative numbers and floats are errors); a missing slot returns the default, and omitting the default is an error. The first round has no values, and a round that ends naturally clears the passed values
- Multi-dimensional arrays use `[expr, ...]` literals and `_` suffixes: `initialize integer a_2 []` declares a 2-D array, `initialize float d_0 3.5` declares a scalar, and `reassign a_1_1 99` writes one cell. Without a dimension suffix, the interpreter infers dimensions from the literal's nesting depth; `_0` means a scalar. Ordinary identifiers must not contain `_`
- Arrays never raise out-of-bounds errors: reading or writing `a_5` / `a_(i+1)` / `b_1_2` automatically materializes the missing path, and skipped cells remain Pace's native unassigned state. Reading a missing leaf raises an unassigned error until a value is written there; missing intermediate paths or subarrays are created as empty arrays
- Array indices are 1-based and must evaluate to a positive integer or a positive float with zero fractional part; 0, negative values, 1.5, strings, booleans, and extra dimensions beyond the object's declared dimension are errors
- Arrays have value semantics and are deep-copied on assignment, nesting, function arguments, return values, and `next` / `obtain`; modifying a copy never affects the source
- Array output uses `[1, true, "hello"]`, nested arrays such as `[1, [2, "p"], true]`, and renders unassigned holes as `unassigned`; `input` supports scalars and full leaf elements such as `input a_5`, but not whole arrays or subarrays
- Memory limits attach to one declaration: `initialize integer a limit 1024`, `initialize integer a 5 limit 1024`, and `initialize integer a limit 1024, integer b limit 32` are all valid. Pace is strictly typed: every variable in a multi-variable `initialize` must repeat its own type keyword; omitting it is a `缺少类型声明` parse error. The expression is evaluated at run time and must be a positive integer or a float with a zero fractional part; the cap is stored in bits, belongs to the variable for its lifetime, and is never shared with another variable
- Memory is estimated as: 64 bit for each `integer` / `float` / `boolean` / `unassigned` slot, 64 bit for each array node, and 64 + 16 × characters for each string (UTF-8 code points). Unassigned holes created by automatic growth count as 64-bit slots
- Every operation that can grow a variable is checked against its cap first: `initialize`, whole-variable `reassign`, `input`, and automatic path growth for array writes and reads. If the post-operation estimate exceeds the cap, nothing is modified (no half-built shells); the interpreter prints `Warning: 变量 '<name>' 超出内存限制 <limit> bit，操作被跳过。` to stderr and continues with the next statement. Warnings are never deduplicated
- An over-limit `initialize` does not store its initial value: it keeps an empty dimension shell when the shell itself fits, otherwise it degrades to the no-value state, and prints the warning
- Deep copies obey the cap: `reassign b a` is skipped when copying `a` would exceed `b`'s limit. `input` consumes its line before the check, so a skipped assignment still advances the input stream
- Reading an array also materializes missing paths; when that growth would exceed the cap it is skipped with the same warning. A missing leaf read then raises the usual unassigned error, while a partial subarray read still returns an empty array without modifying the variable
- Float formatting: shortest round-trippable form, with `.0` appended to integral values

## Not Implemented

REPL, formatter, LSP, scientific notation, hexadecimal literals, `else if`, parameter type annotations, chained aliases.

## Known Limitations

- An empty `repeat`, or a `repeat` with no exit path, loops forever; there is no runtime detection, so termination must be guaranteed by the programmer.
- Function parameters are comma-separated, e.g. `function add a, b`.
- `invoke` arguments are comma-separated primaries; if an argument needs `+ -` or similar, group it with parentheses, e.g. `invoke fib (n - 1)`. Writing `invoke fib n - 1` is parsed as `(invoke fib n) - 1`.
- `alias <command> <new-name>`: the first word is the built-in command being replaced (e.g. `initialize` / `output`) and the second is the new alias, e.g. `alias output say` makes `say` an alias of `output`; `alias` is only allowed at the top level of a function body, and chaining is not allowed (an alias cannot be re-aliased).
- A variable initializer may be omitted: `initialize integer a` and `initialize automatic b` are both valid; an omitted initializer leaves the variable in the "no value" state, so it must be assigned via `reassign` / `input` before use, otherwise a "not yet assigned" error is raised.
- `next` and `obtain` are lexically restricted to the body of a `repeat` (including `if` / `otherwise` blocks nested in it). `obtain` indices are 1-based and are not coerced: a float index such as `1.5` is rejected even when mathematically integral.
- `obtain` with a default is evaluated lazily: the default expression is only evaluated when the requested slot was not supplied by the previous round's `next`.
- Array dimensions are fixed after `initialize`, including for `automatic` variables; an empty `[]` can stand in for a dimensioned shell, but changing dimensions or switching between scalar and array is a dimension error. Non-empty branches must share the same nesting depth, while lengths may differ.
- Pace is strictly typed: every variable in `initialize a, b`-style multi-declarations must carry its own type keyword (`initialize integer a limit 1024, integer b limit 32`); omitting one is a `缺少类型声明` parse error.
- A memory limit is fixed by the `initialize` that created the variable: `automatic` keeps it and `free` keeps it, while `delete` removes it together with the variable; re-initializing the same name in the same frame creates a fresh variable with the new (or absent) limit.
- A limit below 64 bit cannot hold any scalar or array shell; such a variable remains in the no-value state and every later assignment is skipped with a warning. A limit is a per-variable logical budget, not a process-wide cap: function parameters and variables declared without `limit` are unbounded.

## Semantics Decisions Summary

- Platform: Windows, C++17 only.
- Float-to-integer: the fractional part must be 0, otherwise an error (no truncation).
- Numeric overflow: out-of-range float-to-integer, integer `+ - *` overflow, and floating-point overflow (inf) all error.
- Comparisons: type mismatches always error; numbers may mix (promoted to double); strings only compare with strings; booleans only with booleans.
- `invoke` arity is checked at parse time (including forward references).
- Empty blocks: a control statement immediately followed by a statement at the same column is treated as an empty block.
- CLI: `pace.exe run <file.pace>`.
- Scope is bounded by functions; `if` / `repeat` do not introduce new scopes.
- Runtime errors also carry `file:line:col`, are written to stderr, and exit with code 1.
- Warnings are written to stderr and do not affect the exit code.
- Memory limits are logical per-variable bit budgets, estimated with the fixed formula above and checked before any mutation; an over-limit operation is skipped atomically and only emits a `Warning:` line, so it never aborts the program.
