# Pace v0.3.0

[English](#english) | [简体中文](#简体中文)

---

## English

Pace v0.3.0 adds **multi-value state passing between `repeat` iterations**, with a new `next` statement and `obtain` expression.

### Highlights

- **`next` statement** — `next expr1, expr2, ...` evaluates the values, abandons the rest of the current `repeat` iteration, and passes the values to the next round. Think of it as `continue` with a payload.
- **`obtain` expression** — `obtain <index>` or `obtain <index> : <default>` reads a value passed by the previous round's `next` in the nearest `repeat`.
- **Strict context checking** — `next` and `obtain` are rejected at parse time unless they appear lexically inside a `repeat` body (including `if` / `otherwise` blocks nested in it).
- **1-based integer indices** — the index must evaluate to a positive integer. `0`, negative numbers, and floats (including `1.5`) are rejected.
- **Defaults are lazy** — the expression after `:` is only evaluated when the requested slot was not supplied.

### Example

```pace
function main
  initialize integer i 0
  repeat
    reassign i i + 1
    output obtain 1 : 0, " ", obtain 2 : "default", "\n"
    if i < 3
      next i, "hello"
    if i = 4
      exit 2
```

### Semantics

- The first round starts with an empty value list.
- A round that ends naturally (no `next`) clears the value list for the next round.
- `next` values are evaluated left-to-right before control transfers.
- `next` and `obtain` always target the nearest enclosing `repeat`; nested loops do not leak state into each other.
- A missing slot with no default raises a runtime error.

### Compatibility

- `next` and `obtain` are now reserved keywords, and `:` is a new token. Identifiers that previously used these names must be renamed.
- No other syntax or runtime semantics changed.

### Tests & Docs

- Added **6** cases (`repeat_next_obtain`, `obtain_default`, `err_next_outside_repeat`, `err_obtain_outside_repeat`, `err_obtain_no_default`, `err_obtain_invalid_index`).
- Full suite: **55/55 PASS** (21 positive, 32 error, 2 warning).
- English `README.md` and Chinese `README.zh-CN.md` updated with `next` / `obtain` syntax and v0.3.0.
- Maintainer handle updated to **KingBookPride**.

---

## 简体中文

Pace v0.3.0 为 **`repeat` 循环引入多值状态传递机制**，新增 `next` 语句与 `obtain` 表达式。

### 主要变更

- **`next` 语句** —— `next 表达式1, 表达式2, ...` 先求值，再丢弃当前 `repeat` 轮次剩余语句，并把值传给下一轮；可理解为「携带值的 `continue`」。
- **`obtain` 表达式** —— `obtain <索引>` 或 `obtain <索引> : <默认值>`，在最近的 `repeat` 中读取上一轮 `next` 传来的值。
- **严格上下文校验** —— `next` 与 `obtain` 若不在 `repeat` 循环体内（含其中嵌套的 `if` / `otherwise` 块）使用，解析期即报错。
- **1-based 整数索引** —— 索引必须求值为正整数；`0`、负数与浮点数（包括 `1.5`）均报错。
- **默认值惰性求值** —— 仅当对应位置没有传入值时，才求值 `:` 后的表达式。

### 示例

```pace
function main
  initialize integer i 0
  repeat
    reassign i i + 1
    output obtain 1 : 0, " ", obtain 2 : "default", "\n"
    if i < 3
      next i, "hello"
    if i = 4
      exit 2
```

### 运行语义

- 第一轮开始时值列表为空。
- 某一轮自然结束（未执行 `next`）会清空值列表，供下一轮使用。
- `next` 的多个值按从左到右的顺序求值后再转移控制流。
- `next` 与 `obtain` 始终作用于最近的 `repeat`；嵌套循环之间状态互不串扰。
- 对应位置无值且未提供默认值时，运行期报错。

### 兼容性

- `next`、`obtain` 成为保留关键字，`:` 为新增符号；此前用作标识符的代码需要改名。
- 其余语法与运行语义不变。

### 测试与文档

- 新增 **6** 个用例（`repeat_next_obtain`、`obtain_default`、`err_next_outside_repeat`、`err_obtain_outside_repeat`、`err_obtain_no_default`、`err_obtain_invalid_index`）。
- 全量测试：**55/55 PASS**（正例 21、错误例 32、warning 例 2）。
- 英文 `README.md` 与中文 `README.zh-CN.md` 已补充 `next` / `obtain` 语法并更新到 v0.3.0。
- 维护者用户名更新为 **KingBookPride**。

---

**Full Changelog:** compare `v0.2.0...v0.3.0`
