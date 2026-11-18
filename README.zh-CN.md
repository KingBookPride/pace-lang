[English](README.md) | [简体中文](README.zh-CN.md)

# Pace 解释器（C++17）

Pace 是一门缩进敏感、显式类型、支持 `automatic` 自动类型的脚本语言，源文件扩展名为 `.pace`。本仓库是其 C++17 解释器。

> 当前版本：**v0.5.0-rc.1**

## 构建

```powershell
.\build.ps1            # 构建（-std=c++17 -Wall -Wextra -pedantic）
.\build.ps1 -Debug     # 调试构建（-g -O0）
.\build.ps1 -Sanitize  # 尝试 ASan（-fsanitize=address），不可用时自动回退 -Debug
```

产物：仓库根目录 `pace.exe`。

## 运行

```powershell
.\pace.exe run <file.pace>
```

退出码：

| 退出码 | 含义 |
| --- | --- |
| `0` | 正常结束，或 `exit` 主动退出程序 |
| `1` | 词法 / 语法 / 解析期语义 / 运行期错误（诊断写 stderr，格式 `文件:行:列: 错误: 消息`） |
| `2` | CLI 用法错误（缺子命令、非 `run`、缺文件、多余参数、文件不存在） |

示例：

```powershell
.\pace.exe run examples\hello.pace
.\pace.exe run examples\demo.pace
.\pace.exe run examples\loop_and_exit.pace
```

## 测试

```powershell
.\run_tests.ps1
# 或
python tests\run_tests.py
# 指定解释器 / 过滤用例
python tests\run_tests.py --exe .\pace.exe --filter input
```

用例位于 `tests/cases/<name>/`：

| 文件 | 含义 |
| --- | --- |
| `main.pace` | 必填，被测程序 |
| `input.txt` | 可选，stdin |
| `expected.out` | 可选，stdout 精确匹配（统一 CRLF/LF，忽略 BOM） |
| `expected.err_contains` | 可选，每一非空行都必须作为子串出现在 stderr 的某一行 |
| `expected.exit` | 必填，期望退出码 |
| `expected.warning` | 可选，存在即要求 stderr 至少含一条 `: warning: ` |

- stdout 精确比较；`expected.err_contains` 的每个非空行须作为子串出现在 stderr 某一行（不要求顺序）；单例超时 10 秒。
- 错误用例（`expected.exit` = 1）额外要求 stderr 至少一行匹配 `文件:行:列: 错误: `。
- warning 用例要求 stderr 至少一行匹配 `文件:行:列: warning: `，且 stdout 精确匹配、退出码 0。
- 运行期的内存超限告警使用固定纯文本格式 `Warning: 变量 '<变量名>' 超出内存限制 <limit> bit，操作被跳过。`（不带 `文件:行:列` 前缀），通过 `expected.err_contains` 断言。
- 当前共 **88** 个用例：正例 44、错误例 42、warning 例 2。

## 特性

- 缩进敏感词法：空格缩进、统一缩进单位、INDENT/DEDENT、tab 报错
- `\note ... \note` 跨行注释（字符串内不生效）；裸 `\n` 换行字符串
- 类型：`integer` / `float` / `boolean` / `string` / `automatic`
- 变量状态机：`initialize`（支持可选的 `limit <表达式>` 内存上限）/ `reassign` / `free` / `delete` / `automatic`
- 语句：`output` / `input` / `repeat` / `if` / `otherwise` / `exit` / `next` / `return` / `invoke` / `alias`
- 函数：签名预扫描、前向引用、参数个数解析期校验、名称冲突检查（参数 / 变量 / 别名）
- 表达式优先级：`and`/`or`（同级左结合，短路）→ `not`（作用于比较）→ 比较（禁止链式）→ `+ -` → `* /` → 一元 `-` → 基本项
- 整数 `+ - *` 溢出检查；`/` 恒返回 double 且除零报错；浮点 inf 报错
- 字符串拼接：`+` 支持 `string + string`（如 `"a" + "b"` = `"ab"`）；`string` 与数值混合（`string + integer` / `integer + string`）报类型不匹配错误；`automatic` 变量当前存字符串时同样适用
- 浮点转整数须小数部分为 0 且在 int64 范围；字符串与数值比较报错；`string → boolean` 报错
- `exit N` 层级语义（函数体计 1 层，每个外层 `if`/`repeat` 加 1 层）
- `next` 语句：求值一个或多个逗号分隔的表达式，丢弃当前 `repeat` 轮次剩余语句，并把值传给下一轮（类似携带值的 `continue`）；仅允许出现在 `repeat` 内（含其中的 `if`/`otherwise` 块）
- `obtain` 表达式：`obtain <索引>` 或 `obtain <索引> : <默认值>`，在最近的 `repeat` 内读取上一轮 `next` 传来的 1-based 值。索引必须求值为正整数（0、负数、浮点数均报错）；对应位置无值则返回默认值，未提供默认值则报错。第一轮无任何值，某一轮自然结束会清空传入值
- 多维数组使用 `[表达式, ...]` 字面量与 `_` 后缀：`initialize integer a_2 []` 声明二维数组，`initialize float d_0 3.5` 声明标量，`reassign a_1_1 99` 写入一个单元；不写维度后缀时按字面量最大嵌套深度推导，`_0` 表示标量，普通标识符严禁包含 `_`
- 数组不存在越界报错：`a_5`、`a_(i+1)`、`b_1_2` 的读写会自动物化缺失路径，跳过的位置保持 Pace 原生的「无值状态」；完整下标的叶元素未写入前读取会报无值错误，中间层或部分下标会创建空子数组
- 数组下标为 1-based，必须求值为正整数或小数部分为 0 的正浮点数；0、负数、1.5、字符串、布尔值，以及超过对象声明维度的下标均报错
- 数组为值语义，在整体赋值、嵌套、函数传参、返回值及 `next` / `obtain` 时递归深拷贝，修改副本不会影响原数组
- 数组输出格式为 `[1, true, "hello"]`，可嵌套如 `[1, [2, "p"], true]`，无值元素显示为 `unassigned`；`input` 仅支持普通标量或完整叶元素（如 `input a_5`），不支持整数组或子数组
- 只读属性 `_size` / `_quantity` 可位于后缀链末尾：`a_size`、`a_1_2_size`、`a_1_quantity`。属性最多出现一次且只能在末尾；`a_size_1`、`a_size_size`、`a_1_size_2` 报 `属性必须位于下标链末尾`。`_size` 返回当前值的 `estimateValueBits`，单位是 **bit 而不是字节**，超过 `INT64_MAX` 时钳制；`_quantity` 对已赋值标量返回 1、`unassigned` 返回 0，对数组返回 **直接子元素中 quantity > 0 的个数**：

| 值 | `_quantity` | `_size` |
| --- | --- | --- |
| `[1, 2]` | 2 | 192 |
| `[[1], []]` | 1 | 256 |
| `[[], []]` | 0 | 192 |
| `[[]]` | 0 | 128 |
| `[[[1,2],[3,4]], [[5,6]], []]` | 2 | 832 |

- 属性读取严格只读：`a_5` 不存在时 `a_5_size` 返回 `64`、`a_5_quantity` 返回 `0`，且 `a` 的结构完全不变（不触发自动扩容）。下标数量超过声明维度仍报维度冲突。属性是右值，不能作为赋值或声明目标：`reassign a_size 1`、`input a_size`、`free a_size`、`delete a_size`、`initialize integer a_size 1`、`automatic a_size` 均报 `属性 '_size' 是只读的，不能作为赋值或声明目标`（`_quantity` 同文案，仅替换属性名）
- 单位是 **bit 不是字节**：`output a_size / 8` 输出字节数。例如 3 个字符的字符串为 `64 + 3 × 16 = 112` bit（14 字节），`integer` 为 64 bit（8 字节）
- 对带 `limit` 的变量，`a_size > limit` **恒假**：所有超限修改都被原子跳过，`a_size <= limit` 是不变量。需要预算判断时应与自己的阈值比较，例如 `if a_size > 8192`
- 下标位允许裸标识符：`a_i` 等价于 `a_(i)`，`a_i_j` 等价于 `a_(i)_(j)`。每个 `_` 只取一个 token——标识符、整数/浮点字面量或括号表达式——因此 `a_i + 1` 解析为 `(a_i) + 1`；`_` 后出现其他关键字（`size` / `quantity` 除外）报 `'_' 后只允许下标或属性 size/quantity`
- `size` 与 `quantity` 是保留关键字：不能作变量名、函数名、参数名或别名新名，否则报 `关键字不能作为标识符`
- 内存上限属于单个声明：`initialize integer a limit 1024`、`initialize integer a 5 limit 1024`、`initialize integer a limit 1024, integer b limit 128` 均合法。Pace 是强类型语言：多变量 `initialize` 中每个变量都必须重复写出自己的类型关键字，省略即报 `缺少类型声明` 解析错误。`limit` 表达式在运行期求值，必须是 **至少 64** 的正整数或小数部分为 0 的正浮点数（`limit 0`、负数、小数、1–63 均报错）；上限以 bit 为单位，属于变量的永久属性，生命周期内不可修改，也不会共享给其它变量
- 内存估算公式：`integer` / `float` / `boolean` / `unassigned` 每个槽位 64 bit，每个数组节点 64 bit，每个字符串 64 + 16 × 字符数（按 UTF-8 码点计）；自动扩容产生的无值洞同样按 64 bit 计入，根变量的无值状态也按一个 64 bit 槽位计入
- 任何可能增大变量占用的操作都先与上限比较：`initialize`、整体 `reassign`、`input`、数组写入与读取时的自动扩容。若操作后的预估值超限，则不做任何修改（绝不留下半建成的空壳），向 stderr 打印 `Warning: 变量 '<变量名>' 超出内存限制 <limit> bit，操作被跳过。`，然后继续执行下一条语句；告警不去重，每次触发都打印
- `initialize` 的初值本身超限时不赋予初值：能容纳维度空壳时保留空壳，否则降级为无值状态，并打印同样的告警
- 深拷贝同样受上限约束：`reassign b a` 若拷贝 `a` 会使 `b` 超限，则整个赋值作废。`input` 在检查前已消费文本行，因此被跳过的输入仍会推进输入流
- 读取数组时也会物化缺失路径；若该扩容会超限，则跳过扩容并打印告警：完整叶元素随后照常报无值错误，部分下标仍返回空子数组且不改变变量
- 浮点格式化：最短可回读表示，整数值补 `.0`

## 未实现

REPL、格式化器、LSP、科学计数法、十六进制、`else if`、参数类型标注、链式别名。

## 已知限制

- 空 `repeat` 或没有退出路径的 `repeat` 会无限循环；不做运行期检测，请自行保证循环可终止。
- 函数参数以逗号分隔，例如 `function add a, b`。
- `invoke` 的实参是逗号分隔的基本项；实参内部若需 `+ -` 等运算请用括号分组，例如 `invoke fib (n - 1)`。裸写 `invoke fib n - 1` 会被解析为 `(invoke fib n) - 1`。
- `alias 原命令 新名`：第一个词是被替换的内建命令（如 `initialize` / `output`），第二个词是新别名，例如 `alias output say` 让 `say` 成为 `output` 的别名；`alias` 只允许出现在函数体顶层，且不允许链式（别名不能再被别名）。
- 变量声明初值可省略：`initialize integer a` 或 `initialize automatic b` 均合法，省略初值时变量处于「无值状态」，使用前必须先 `reassign` / `input` 赋值，否则报「尚未赋值」。
- `next` 与 `obtain` 在词法上只允许位于 `repeat` 循环体内（含嵌套其中的 `if` / `otherwise` 块）。`obtain` 索引为 1-based 且不做类型转换：`1.5` 这类浮点索引即使数学上是整数也会被拒绝。
- `obtain` 的默认值为惰性求值：只有当上一轮 `next` 未提供对应位置的值时，才会求值 `:` 后的默认表达式。
- 数组维度在 `initialize` 后固定，包括 `automatic` 变量；空 `[]` 可充当对应维度的空壳，但改变维度或在标量与数组之间切换属于维度错误。非空分支的嵌套深度必须一致，各层长度可以不同。
- Pace 是强类型语言：`initialize` 多变量声明中的每个变量都必须带自己的类型关键字（`initialize integer a limit 1024, integer b limit 128`），省略即报 `缺少类型声明` 解析错误。
- 内存上限由创建该变量的 `initialize` 固定：`automatic` 与 `free` 都保留上限，`delete` 随变量一起移除；在同一帧内重新 `initialize` 同名变量会创建新变量，采用新的（或省略的）上限。
- `limit` 必须至少 64 bit：1–63 报 `limit 不能小于 64 bit（变量本身占用 64 bit 槽位）`，0、负数与非整值浮点仍报「必须为正整数」错误。省略 `limit`（内部 0 哨兵）表示不限制；0 哨兵不是语言层可写取值。上限是单变量的逻辑预算，不是进程级内存上限；函数参数与未声明 `limit` 的变量不受限制。
- `size` / `quantity` 是硬关键字：没有软保留，也不会 warning 后继续。`_size` / `_quantity` 是只读右值，不能作为赋值或声明目标。

## 语义决策摘要

- 平台：Windows，仅 C++17。
- 浮点转整数：小数部分必须为 0，否则报错（不截断）。
- 数值溢出：浮点转整数越界、整数 `+ - *` 越界、浮点上溢（inf）均报错。
- 比较：类型不匹配一律报错；数值可混合（提升 double）；字符串只与字符串；布尔只与布尔。
- `invoke` 参数个数在解析期检查（含前向引用）。
- 空块：控制语句后紧跟同列语句视为空块。
- CLI：`pace.exe run <file.pace>`。
- 作用域以函数为界；`if` / `repeat` 不新建作用域。
- 运行时错误同样带 `文件:行:列`，写 stderr，退出码 1。
- warning 写 stderr，不影响退出码。
- 内存上限是按上述固定公式估算的单变量 bit 预算，在任何修改前检查；超限操作原子跳过且只打印一行 `Warning:`，不会中止程序。
- `_size` 与 limit 判定使用同一个 `estimateValueBits`；属性读取不扩容、不物化、不修改任何变量。
