# Pace v0.5.0-rc.1

[English](#english) | [简体中文](#简体中文)

---

## English

Pace v0.5.0-rc.1 adds read-only `_size` / `_quantity` attributes, bare identifier subscripts, and a unified 64-bit slot accounting for `limit`.

### Breaking changes

- **`size` and `quantity` are now hard keywords.** They cannot be used as variable, function, parameter, or alias names. Any old program using them as identifiers now fails with `关键字不能作为标识符`.
- **The minimum `limit` is 64 bit.** Values 1–63 now raise `limit 不能小于 64 bit（变量本身占用 64 bit 槽位）`. `limit 0`, negatives and non-integral floats keep the previous "must be a positive integer" error; omitting `limit` still means unlimited.
- **A no-value variable now counts as 64 bit in limit accounting.** Previously the root no-value state was treated as 0 bit; it is now the same 64-bit `unassigned` slot used everywhere else. Because every limit is at least 64, a freshly declared no-value variable still fits its own budget.

### Highlights

- **Read-only attributes**
  - `a_size`, `a_1_2_size`, `a_1_quantity`.
  - `_size` returns `estimateValueBits` in **bits**, clamped to `INT64_MAX`.
  - `_quantity`: assigned scalar → `1`; `unassigned` → `0`; array → number of direct children whose quantity is greater than 0.
  - Attributes must be the last element of the suffix chain and may appear at most once: `a_size_1`, `a_size_size`, and `a_1_size_2` raise `属性必须位于下标链末尾`.
  - Attributes are rvalues. Using one as a target raises `属性 '_size' 是只读的，不能作为赋值或声明目标` (same for `_quantity`). Covered positions: `reassign`, `input`, `free`, `delete`, `initialize` declaration name, `automatic`.

| Value | `_quantity` | `_size` |
| --- | --- | --- |
| `[1, 2]` | 2 | 192 |
| `[[1], []]` | 1 | 256 |
| `[[], []]` | 0 | 192 |
| `[[]]` | 0 | 128 |
| `[[[1,2],[3,4]], [[5,6]], []]` | 2 | 832 |

- **Strictly read-only evaluation**
  - Attribute reads never expand, materialize or write back any path.
  - `a_5_size` returns `64` and `a_5_quantity` returns `0` when `a_5` is missing, and `a` is unchanged.
  - More indices than the declared dimension still raises the existing dimension conflict, even if the path is missing.

- **Bare identifier subscripts**
  - `a_i` is exactly `a_(i)`; `a_i_j` is `a_(i)_(j)`.
  - Each `_` consumes exactly one token: identifier, integer/float literal, or parenthesized expression.
  - `a_i + 1` parses as `(a_i) + 1`; the `+ 1` is never swallowed into the subscript.
  - A keyword after `_` (other than `size` / `quantity`) raises `'_' 后只允许下标或属性 size/quantity`.

- **Unified limit accounting**
  - `_size` and limit enforcement use the same `estimateValueBits`: no second formula.
  - The 64 / 16 constants, recursive summation, and saturation behavior are unchanged.
  - `limit` values below one 64-bit slot are rejected at declaration time.

### Example

```pace
function main
  initialize automatic a [1, 2]
  output a_size, " ", a_quantity, "\n"      \note 192 2 \note
  output a_5_size, " ", a_5_quantity, "\n"  \note 64 0 (no expansion) \note
  output a_size / 8                          \note 24 bytes \note
```

### Compatibility

- Rename any identifier named `size` or `quantity`.
- Replace `limit n` with `limit n` where `n >= 64`; omit `limit` for an unbounded variable.
- `a_i` is a new accepted spelling; existing `a_(i)` programs are unaffected.
- `_size` / `_quantity` are read-only and cannot appear on the left side of a mutation.

### Verification

- All **88** cases pass: 44 positive, 42 error, 2 warning (73 previous + 15 new).
- `interp_smoke`: **73/73 PASS**; `parser_smoke` and `lexer_smoke` pass.
- `pace.exe` builds with `-Wall -Wextra -pedantic` and no warnings; all examples run.

---

## 简体中文

Pace v0.5.0-rc.1 新增只读 `_size` / `_quantity` 属性、裸标识符下标，并统一 `limit` 的 64 bit 槽位口径。

### Breaking changes（破坏性变更）

- **`size` 与 `quantity` 成为硬关键字。** 不能再作变量名、函数名、参数名或别名新名；旧程序若用作标识符会报 `关键字不能作为标识符`。
- **`limit` 下限提升为 64 bit。** 1–63 现在报 `limit 不能小于 64 bit（变量本身占用 64 bit 槽位）`。`limit 0`、负数与非整值浮点仍报原有的「必须为正整数」错误；省略 `limit` 仍表示不限制。
- **无值变量在 limit 判定中改按 64 bit 计。** 过去根变量的无值状态按 0 bit 处理，现在与其它 `unassigned` 槽位一致按 64 bit 计。由于 limit 下限为 64，新声明的无值变量仍必然满足自身预算。

### 主要变更

- **只读属性**
  - `a_size`、`a_1_2_size`、`a_1_quantity`。
  - `_size` 返回 `estimateValueBits`，单位为 **bit**，超过 `INT64_MAX` 时钳制。
  - `_quantity`：已赋值标量 → `1`；`unassigned` → `0`；数组 → 直接子元素中 quantity 大于 0 的个数。
  - 属性必须在后缀链末尾且最多出现一次：`a_size_1`、`a_size_size`、`a_1_size_2` 报 `属性必须位于下标链末尾`。
  - 属性是右值。作为目标时报 `属性 '_size' 是只读的，不能作为赋值或声明目标`（`_quantity` 同文案）。覆盖 `reassign`、`input`、`free`、`delete`、`initialize` 声明名、`automatic`。

| 值 | `_quantity` | `_size` |
| --- | --- | --- |
| `[1, 2]` | 2 | 192 |
| `[[1], []]` | 1 | 256 |
| `[[], []]` | 0 | 192 |
| `[[]]` | 0 | 128 |
| `[[[1,2],[3,4]], [[5,6]], []]` | 2 | 832 |

- **严格只读求值**
  - 属性读取绝不扩容、不物化、不写回任何路径。
  - `a_5` 不存在时 `a_5_size` 返回 `64`、`a_5_quantity` 返回 `0`，且 `a` 完全不变。
  - 下标数量超过声明维度时仍报既有维度冲突，即使路径缺失。

- **裸标识符下标**
  - `a_i` 等价于 `a_(i)`；`a_i_j` 等价于 `a_(i)_(j)`。
  - 每个 `_` 只取一个 token：标识符、整数/浮点字面量或括号表达式。
  - `a_i + 1` 解析为 `(a_i) + 1`，`+ 1` 绝不会被吞进下标。
  - `_` 后出现其他关键字（`size` / `quantity` 除外）报 `'_' 后只允许下标或属性 size/quantity`。

- **统一 limit 口径**
  - `_size` 与 limit 判定使用同一个 `estimateValueBits`，不存在第二套计算。
  - 64 / 16 常量、递归累加与饱和行为保持不变。
  - 小于一个 64 bit 槽位的 `limit` 在声明期直接拒绝。

### 示例

```pace
function main
  initialize automatic a [1, 2]
  output a_size, " ", a_quantity, "\n"      \note 192 2 \note
  output a_5_size, " ", a_5_quantity, "\n"  \note 64 0（不扩容） \note
  output a_size / 8                          \note 24 字节 \note
```

### 兼容性

- 名为 `size` 或 `quantity` 的标识符需改名。
- `limit n` 需满足 `n >= 64`；不限制时省略 `limit`。
- `a_i` 是新增写法，既有 `a_(i)` 程序不受影响。
- `_size` / `_quantity` 为只读，不能出现在任何变更目标位置。

### 验证

- **88** 个用例全部通过：正例 44、错误例 42、warning 例 2（原 73 + 新增 15）。
- `interp_smoke`：**73/73 PASS**；`parser_smoke`、`lexer_smoke` 通过。
- `pace.exe` 以 `-Wall -Wextra -pedantic` 构建无警告；所有示例运行正常。

---

**Full Changelog:** compare `v0.4.1...v0.5.0-rc.1`
