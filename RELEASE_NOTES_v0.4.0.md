# Pace v0.4.0

[English](#english) | [简体中文](#简体中文)

---

## English

Pace v0.4.0 introduces **multi-dimensional arrays with automatic growth (no out-of-bounds)**.

### Highlights

- **Array syntax** — `[expr, ...]` literals and `_` suffixes. For example `initialize integer a_2 []` declares a 2-D array; `initialize float d_0 3.5` declares a scalar; `reassign a_1_1 99` writes one cell. Ordinary identifiers cannot contain `_`.
- **No out-of-bounds errors** — reading or writing `a_5`, `a_(i+1)`, or `b_1_2` materializes the missing path in place. Skipped cells stay in Pace's native unassigned state; they are never filled with 0, 0.0, `false`, or an empty string.
- **Unassigned propagation** — reading a missing leaf reports that `a_5` is unassigned until a value is written there. Partial subscripts create and return empty subarrays. Intermediate layers are arrays, not fake scalar defaults.
- **Dimensions** — `_0` is a scalar; any subscript on a scalar, including `d_0` and `d_1`, is an error. Without a dimension suffix, dimensions are inferred from the array literal's nesting depth; `[]` is a valid shell for any positive dimension. Changing dimensions after `initialize` is an error.
- **Indices** — all indices are 1-based and must evaluate to a positive integer or a positive float with zero fractional part. Extra indices beyond the object's declared dimension are rejected.
- **Deep copies** — assignment, nesting, function parameters, return values, `next`, and `obtain` recursively copy arrays and preserve unassigned cells, so copies are independent.
- **Formatting** — arrays print as `[1, true, "hello"]`, strings inside arrays are quoted, nested arrays are supported, and unassigned holes print as `unassigned`.
- **Input** — `input` supports scalars and full leaf elements such as `input a_5`; whole arrays and subarray inputs are rejected before stdin is consumed.

```pace
function main
  initialize integer a_2 []
  reassign a_1_1 99
  output a
```

Output:

```text
[[99]]
```

### Compatibility

- The `_`, `[`, and `]` characters now have structural meaning. Identifiers containing underscores must be renamed.
- Automatic variables still have fixed dimensions after declaration; changing rank or switching between scalar and array is a dimension error.
- `[1, [2]]` remains invalid because non-empty branches must share the same nesting depth. `automatic` may still mix scalar leaf types, such as `[1, "x", true]`.

### Tests & Docs

- Added 7 cases: `array_auto_expand_write`, `err_array_read_unset`, `array_read_after_write`, `array_deep_copy`, `array_dynamic_index`, `err_array_dim_conflict`, and `err_array_scalar_index`.
- Full suite: **62/62 PASS** — 25 positive, 35 error, 2 warning.
- `pace.exe` and `tests/membatch.exe` rebuilt with `-Wall -Wextra -pedantic` and no new warnings; interpreter smoke coverage extended to 52 checks.
- English `README.md` and Chinese `README.zh-CN.md` updated to v0.4.0.

---

## 简体中文

Pace v0.4.0 引入**多维数组与全自动扩容（无越界错误）**。

### 主要变更

- **数组语法** —— 支持 `[表达式, ...]` 字面量与 `_` 后缀，例如 `initialize integer a_2 []` 声明二维数组，`initialize float d_0 3.5` 声明标量，`reassign a_1_1 99` 写入一个单元；普通标识符不能包含 `_`。
- **无越界错误** —— 读写 `a_5`、`a_(i+1)`、`b_1_2` 时会就地物化缺失路径；跳过的位置保持 Pace 原生的「无值状态」，绝不填入 0、0.0、`false` 或空字符串。
- **无值传播** —— 完整下标的叶元素未写入前读取会报 `a_5` 无值；部分下标会创建并返回空子数组；中间层始终是数组，而不是伪造的标量默认值。
- **维度规则** —— `_0` 表示标量；标量的任何下标（包括 `d_0`、`d_1`）都报错。无维度后缀时按数组字面量最大嵌套深度推导，`[]` 可充当任意正整数维度空壳；`initialize` 后不允许改变维度。
- **下标规则** —— 全部下标为 1-based，必须求值为正整数或小数部分为 0 的正浮点数；下标数超过对象声明维度即报错。
- **深拷贝** —— 赋值、嵌套、函数传参、返回值、`next` 与 `obtain` 都会递归复制数组并保留无值元素，副本彼此独立。
- **打印格式** —— 数组以 `[1, true, "hello"]` 格式输出，内部字符串带双引号，支持嵌套数组；无值元素显示为 `unassigned`。
- **input** —— `input` 支持普通标量与完整叶元素，例如 `input a_5`；整数组与子数组输入会在读取 stdin 前拒绝。

```pace
function main
  initialize integer a_2 []
  reassign a_1_1 99
  output a
```

输出：

```text
[[99]]
```

### 兼容性

- `_`、`[`、`]` 现在具有结构性含义，包含下划线的旧标识符需要改名。
- `automatic` 变量同样固定声明后的维度；改变维度或在标量与数组之间切换属于维度错误。
- `[1, [2]]` 仍不合法，因为非空分支的嵌套深度必须一致；`automatic` 仍可混合标量叶类型，如 `[1, "x", true]`。

### 测试与文档

- 新增 7 个用例：`array_auto_expand_write`、`err_array_read_unset`、`array_read_after_write`、`array_deep_copy`、`array_dynamic_index`、`err_array_dim_conflict`、`err_array_scalar_index`。
- 全量测试：**62/62 PASS**（正例 25、错误例 35、warning 例 2）。
- 已用 `-Wall -Wextra -pedantic` 重新构建 `pace.exe` 与 `tests/membatch.exe`，无新增警告；解释器冒烟测试扩展至 52 项。
- 英文 `README.md` 与中文 `README.zh-CN.md` 已更新至 v0.4.0。

---

**Full Changelog:** compare `v0.3.0...v0.4.0`
