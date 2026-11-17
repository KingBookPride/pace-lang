# Pace v0.4.1

[English](#english) | [简体中文](#简体中文)

---

## English

Pace v0.4.1 adds **per-variable memory limits** with **atomic over-limit rollback**. Every operation that could grow a variable is estimated in bits before it runs; if the cap would be exceeded, nothing is modified, a warning is printed, and the program continues.

### Highlights

- **`limit` clause on declarations**
  - `initialize integer a limit 1024`
  - `initialize integer a 5 limit 1024`
  - `initialize integer a limit 1024, integer b limit 32`
  - Pace remains strictly typed: every variable declaration must repeat its own type keyword. Writing `initialize integer a limit 1024, b limit 32` is a `缺少类型声明` parse error.
  - The expression is evaluated at run time and must be a positive integer or a float with a zero fractional part; `limit 0`, negatives and `2.5` are errors.
  - The cap is a permanent property of the variable: `automatic` and `free` keep it, `delete` removes it with the variable.

- **Fixed memory model (bit)**
  - `integer` / `float` / `boolean` / `unassigned` slot: **64**
  - `string`: **64 + 16 × characters** (UTF-8 code points)
  - Array node: **64**, plus all elements recursively
  - Holes created by automatic growth are real `std::vector` slots and count as 64-bit unassigned slots.

- **Atomic enforcement**
  - Checked before `initialize`, whole-variable `reassign`, `input`, and automatic path growth for array writes and reads.
  - If the estimated post-operation size exceeds the cap, the operation is skipped with no partial shells and no state change:
    `Warning: 变量 '<name>' 超出内存限制 <limit> bit，操作被跳过。`
  - Warnings go to stderr, are never deduplicated, and never change the exit code.

- **Initialization degradation**
  - An over-limit initializer is not stored. The variable keeps an empty dimension shell when the shell fits, otherwise it degrades to the no-value state, and prints the warning.

- **Deep copies and input**
  - `reassign b a` is skipped when copying `a` would exceed `b`'s cap.
  - `input` consumes its line before the check, so a skipped assignment still advances the input stream.

- **Reads**
  - Reads still materialize missing paths. When that growth would exceed the cap it is skipped with the same warning: a missing leaf then raises the usual unassigned error, while a partial subarray read returns an empty array without modifying the variable.

### Compatibility

- Pace remains strictly typed: a multi-variable `initialize` requires a type keyword for every variable; omitting one is a `缺少类型声明` error.
- `limit` is now a reserved keyword; identifiers named `limit` must be renamed.
- A variable whose cap is below 64 bit cannot hold any scalar or array shell; it stays in the no-value state and later assignments are skipped with warnings.
- A limit is a per-variable logical budget, not a process-wide memory cap.
- No other syntax or semantics changed.

### Example

```pace
function main
  initialize integer a_2 [] limit 200
  reassign a_1_1 5
  output a, "\n"          \note [[5]] \note
  reassign a_10_10 7      \note estimate 1408 bit > 200: skipped atomically \note
  output a                \note still [[5]] \note
```

### Verification

- All **73** cases pass: 33 positive, 38 error, 2 warning (previous 62 plus 11 new limit cases).
- `interp_smoke`: **61/61 PASS**, including exact boundaries, atomic rollback, degradation, deep-copy caps, consumed input, and UTF-8 string sizing.
- `pace.exe` rebuilt with `-Wall -Wextra -pedantic` and no warnings.

---

## 简体中文

Pace v0.4.1 引入**单变量内存上限（limit）**与**超限原子回滚**。任何可能增大变量占用的操作都会先按 bit 估算；若会超限，则不修改任何数据、打印告警并继续执行。

### 主要变更

- **声明上的 `limit` 子句**
  - `initialize integer a limit 1024`
  - `initialize integer a 5 limit 1024`
  - `initialize integer a limit 1024, integer b limit 32`
  - Pace 保持强类型：每个变量声明都必须重复写出自己的类型关键字。写成 `initialize integer a limit 1024, b limit 32` 会报 `缺少类型声明` 解析错误。
  - 表达式在运行期求值，必须是正整数或小数部分为 0 的正浮点数；`limit 0`、负数、`2.5` 均报错。
  - 上限是变量的永久属性：`automatic` 与 `free` 保留，`delete` 随变量移除。

- **固定内存模型（bit）**
  - `integer` / `float` / `boolean` / `unassigned` 槽位：**64**
  - `string`：**64 + 16 × 字符数**（按 UTF-8 码点计）
  - 数组节点：**64**，再加全部元素递归之和
  - 自动扩容产生的洞是真实的 `std::vector` 槽位，按 64 bit 的无值槽位计入。

- **原子化拦截**
  - 在 `initialize`、整体 `reassign`、`input`、数组写入与读取的自动扩容之前检查。
  - 若操作后的预估占用超限，则整个操作作废：不留下半建成的空壳，变量保持原状，并打印
    `Warning: 变量 '<变量名>' 超出内存限制 <limit> bit，操作被跳过。`
  - 告警写 stderr，不去重，不影响退出码。

- **初始化降级**
  - 初值本身超限时不赋予初值：空壳能容纳时保留维度空壳，否则降级为无值状态，并打印告警。

- **深拷贝与 input**
  - `reassign b a` 若拷贝 `a` 会使 `b` 超限，则整个赋值作废。
  - `input` 在检查前已消费文本行，被跳过的赋值仍会推进输入流。

- **读取**
  - 读取仍会物化缺失路径；若扩容会超限则跳过并打印同样告警：完整叶元素随后照常报无值错误，部分下标返回空子数组且不改变变量。

### 兼容性

- Pace 保持强类型：多变量 `initialize` 中每个变量都必须带类型关键字，省略即报 `缺少类型声明`。
- `limit` 成为保留关键字；名为 `limit` 的标识符需改名。
- 上限小于 64 bit 时无法容纳任何标量或数组空壳，变量会一直处于无值状态，后续赋值均被跳过并告警。
- 上限是单变量的逻辑预算，不是进程级内存上限。
- 其余语法与语义不变。

### 示例

```pace
function main
  initialize integer a_2 [] limit 200
  reassign a_1_1 5
  output a, "\n"          \note [[5]] \note
  reassign a_10_10 7      \note 预估 1408 bit > 200：原子跳过 \note
  output a                \note 仍为 [[5]] \note
```

### 验证

- **73** 个用例全部通过：正例 33、错误例 38、warning 例 2（原 62 个 + 新增 11 个 limit 用例）。
- `interp_smoke`：**61/61 PASS**，覆盖精确边界、原子回滚、初始化降级、深拷贝上限、输入消费与 UTF-8 字符串估算。
- `pace.exe` 以 `-Wall -Wextra -pedantic` 重编译，无警告。

---

**Full Changelog:** compare `v0.4.0...v0.4.1`
