# Pace v0.2.0

[English](#english) | [简体中文](#简体中文)

---

## English

Pace v0.2.0 is a **renaming release**: the language formerly known as *Fluid* is now **Pace**, and source files move from `.fld` to `.pace`.

### ⚠️ Breaking changes

- **Language renamed:** Fluid → **Pace**.
- **File extension:** `.fld` → **`.pace`**.
- **Executable:** `fluid.exe` → **`pace.exe`**.
- **CLI:** `pace.exe run <file.pace>`.

### Highlights

- Global rename across sources, build/test scripts, examples, and all test cases.
- `build.ps1` now produces `pace.exe`.
- Test harness (`run_tests.py`, `run_tests_memory.py`, `parser_cli_smoke.py`) updated to `.pace`.
- Documentation rewritten in **English** (`README.md`) with a matching **Simplified Chinese** version (`README.zh-CN.md`); both start with a language switcher.
- Version bumped to **v0.2.0**.

### Migration

1. Rename source files: `*.fld` → `*.pace`.
2. Replace invocations: `fluid.exe run app.fld` → `pace.exe run app.pace`.
3. Nothing else changes semantically; `alias` uses `alias <command> <new-name>` (e.g. `alias output say`).

### Verification

- All **49** test cases pass: 19 positive, 28 error, 2 warning.
- All three examples (`hello`, `demo`, `loop_and_exit`) run correctly.

---

## 简体中文

Pace v0.2.0 是一次**更名版本**：原 *Fluid* 语言正式更名为 **Pace**，源文件扩展名由 `.fld` 改为 `.pace`。

### ⚠️ 破坏性变更

- **语言更名：** Fluid → **Pace**。
- **文件扩展名：** `.fld` → **`.pace`**。
- **可执行文件：** `fluid.exe` → **`pace.exe`**。
- **命令行：** `pace.exe run <file.pace>`。

### 主要变更

- 源码、构建/测试脚本、示例与全部测试用例全局更名。
- `build.ps1` 产物改为 `pace.exe`。
- 测试驱动（`run_tests.py`、`run_tests_memory.py`、`parser_cli_smoke.py`）后缀匹配改为 `.pace`。
- 文档改写为**英文版**（`README.md`），并新增对应的**简体中文版**（`README.zh-CN.md`）；两版首行均带语言切换链接。
- 版本号升至 **v0.2.0**。

### 升级指南

1. 重命名源文件：`*.fld` → `*.pace`。
2. 替换调用：`fluid.exe run app.fld` → `pace.exe run app.pace`。
3. 语义无其它变化；`alias` 语法为 `alias 原命令 新名`（如 `alias output say`）。

### 验证

- **49** 个测试用例全部通过：正例 19、错误例 28、warning 例 2。
- 三个示例（`hello`、`demo`、`loop_and_exit`）均正常运行。

---

**Full Changelog:** compare `v0.1.x...v0.2.0`
