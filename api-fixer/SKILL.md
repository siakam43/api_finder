---
name: api-fixer
description: Use when the user invokes /api-fixer or asks to fix stale file paths, remove deleted interfaces, or correct an external API list (old_api.json) in embedded/low-level C codebases (Linux kernel drivers, UEFI/BL31/BL2/XLoader firmware, ISP/SensorHub/GPU co-processor firmware).
---

# api-fixer

消费历史版本中识别出的外部接口列表（old_api.json），与当前代码分析范围做交叉比对，修复路径错误，排除已删除的接口，输出修正后的外部接口列表。适用业务场景：嵌入式/底层系统代码（Linux 内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU 固件）。

## 核心原则

1. **分析质量优先于分析效率。** 宁可慢，不可草率。每个 old_api 条目必须认真验证，不得因条目多而加速跳读。

2. **更少的误报优先于更少的漏报。** 不确定函数定义是否存在时，倾向淘汰而非保留。只在有充分证据（两步法确认函数定义存在）时才保留接口。

## 使用方法

```
/api-fixer <project_dir>
```

如果未指定 `project_dir`，默认为当前工作目录。

## 输出

分析完成后在 `<project_dir>/.ethunter_out/api-fixer/` 下生成：

- `inherited_apis.json` — 修复后的外部接口列表
- `progress.json` — 断点续分析状态 + 逐条目处理结果

---

## 一、初始化与分析范围确定

### Step 1: 解析输入参数

`project_dir` = 用户指定的路径，如果未指定则使用当前工作目录。

### Step 2: 确定代码分析范围

检查 `<project_dir>/.ethunter_out/` 目录：

**情况A — clean_code.txt 存在：**

用两种方法确认 `<project_dir>/.ethunter_out/clean_code.txt` 是否存在：
- 方法一：`ls <project_dir>/.ethunter_out/clean_code.txt`
- 方法二：`Read` 工具尝试读取

确认存在后，读取 clean_code.txt，每一行是一个文件路径。路径可能是绝对路径，也可能是相对于 project_dir 的相对路径。

处理步骤：
1. 对每一行路径进行判断：
   - 以 `/` 开头 → 绝对路径，直接使用
   - 不以 `/` 开头 → 相对路径，拼接为 `<project_dir>/<相对路径>`
2. 逐一检查拼接后的路径是否实际存在于磁盘上，不存在的文件忽略即可，不影响分析流程。
3. 去重（可能存在重复路径）。
4. 去重后的文件列表作为分析范围（scope_files）。

**情况B — clean_code.txt 不存在但 .etignore 存在：**

用两种方法确认 `<project_dir>/.ethunter_out/.etignore` 是否存在。确认存在后，读取 .etignore，理解其排除规则（语法与 .gitignore 一致）。然后运行以下命令收集 project_dir 下全部 .c 和 .h 文件：

```bash
find <project_dir> -type f \( -name "*.c" -o -name "*.h" \)
```

将结果与 .etignore 规则进行匹配，排除被忽略的文件或目录下的文件，剩余文件作为分析范围。

**情况C — 两者都不存在：**

运行以下命令：

```bash
find <project_dir> -type f \( -name "*.c" -o -name "*.h" \)
```

收集全部 .c 和 .h 文件作为分析范围。

### Step 3: 检查输入文件

用两种方法交叉确认 old_api.json 是否存在。**该文件必须存在，缺失则报错退出（无输入数据无法继续分析）：**

- `<project_dir>/.ethunter_out/old_api.json` — 历史版本中识别出的外部接口列表

两种检查方法：
- 方法一：`ls <文件路径>`
- 方法二：`Read` 工具尝试读取

**判断规则：**

| 两次检查结果 | 处理方式 |
|-------------|---------|
| 都确认文件存在 | 文件存在，正常加载 |
| 都确认文件不存在（明确报错，如 "No such file or directory"、"NOT_FOUND"） | 文件不存在，**报错退出** |
| 一次存在、一次不存在 | 结果存在疑似，再使用第三种方法检查，取多数结果 |
| 两次都无输出或都报错 | **立即停止分析，向用户报告问题** |

### Step 4: 检测 CodeGraph 可用性

用两种方法检查 `<project_dir>/.codegraph` 目录是否存在。如果存在，在后续所有代码分析中**优先使用 MCP 工具 `codegraph_explore`** 进行代码搜索和理解。如果不存在、或环境中未安装 codegraph 或未配置 MCP，使用常规工具（grep、find、Read），不中断分析。

### Step 5: 初始化输出目录

```bash
mkdir -p <project_dir>/.ethunter_out/api-fixer
```

---

## 二、任务恢复机制

### 文件存在性检查规则（全流程通用）

**禁止使用 `test -f` 或 `[ -f ]` 判断文件是否存在。** 每次需要判断关键文件是否存在时，必须使用至少两种不同方法交叉确认：

| 方法 | 示例 |
|------|------|
| `ls` 检查 | `ls <文件路径>` |
| `find` 检查 | `find <目录> -name "<文件名>" -maxdepth 1` |
| `Read` 工具 | 直接尝试 Read 读取文件 |

**判断规则：**

| 两次检查结果 | 处理方式 |
|-------------|---------|
| 都确认文件存在 | 文件存在，正常加载/恢复 |
| 都确认文件不存在（明确报错，如 "No such file or directory"、"NOT_FOUND"） | 文件不存在，正常跳过/新建 |
| 一次存在、一次不存在 | 结果存在疑似，再使用第三种方法检查，取多数结果 |
| 两次都无输出或都报错 | **立即停止分析，向用户报告问题。不要假设文件缺失后继续分析！** |

### progress.json 结构

```json
{
  "phase": "processing|done",
  "total": 15,
  "processed": 0,
  "results": [
    {"name": "func_a", "original_file": "old_api原始值", "file": "最终绝对路径",
     "result": "inherited|eliminated", "path_updated": true|false,
     "reason": null|"<处理原因>"},
    ...
  ]
}
```

`results` 中每个条目的字段含义与 api-finder 的 inherit_result.json 一致：
- `name`：函数名
- `original_file`：old_api.json 中的原始 file 值
- `file`：最终确定的文件绝对路径
- `result`：`"inherited"`（继承）或 `"eliminated"`（淘汰）
- `path_updated`：`true` 表示路径在 fallback 搜索中被更新，`false` 表示路径未变
- `reason`：淘汰原因（`result = "inherited"` 时为 `null`；`result = "eliminated"` 时为具体淘汰理由）

### 入口恢复流程

在处理任何条目之前，首先执行恢复检查：

```
1. 用两种方法检查 <project_dir>/.ethunter_out/api-fixer/progress.json 是否存在。
   根据判断规则：
   ├── 确认不存在 → 全新分析，初始化 progress.json：
   │     phase = "processing", total = old_api.json 条目数,
   │     processed = 0, results = []
   └── 确认存在 → 加载 progress.json

2. 如果 phase = "done"：分析已完成，告知用户并停止。
   用户可以手动删除 progress.json 后重新分析。

3. 遍历 old_api.json 每个条目，将 name 和 file 原始值与 progress.results 中
   name 和 original_file 对比：
   ├── 已在 results 中 → 保留已处理结果，跳过
   └── 不在 results 中 → 作为待处理条目

4. 待处理条目为空 → 全部已处理，跳到"输出结果"步骤
   待处理条目非空 → 继续第三节
```

---

## 三、外部接口列表修复

将历史版本中已识别的外部接口与当前代码分析范围做交叉比对。核心逻辑与 api-finder Section 4 一致。

### 搜索函数定义的方法

当需要确认某个函数名在指定目标中是否存在**函数体定义**（而非声明或引用）时，使用以下两步法：

1. **grep 定位：** `grep -rn "<函数名>" <目标路径>` — 获取所有匹配该函数名的行及行号。`<目标路径>` 为文件时搜索该文件，为目录时递归搜索。

2. **Read 验证：** 对每个候选匹配，Read 读取匹配行前后各 3 行（上下文共约 7 行），在窗口中判断是否满足以下**全部条件**：
   - 包含完整的函数签名（返回类型 + 函数名 + 参数列表）
   - 签名之后出现 `{`（允许 `{` 在签名行的后续行出现，如 Allman 风格将 `{` 放在下一行）
   - 不以 `;` 结尾（排除函数声明）
   - 不在注释中
   全部满足 → 确认存在函数体定义。任一不满足 → 不是定义，继续检查下一个候选。所有候选都不满足 → 未找到。

### 流程

```
1. 读取 old_api.json。其格式为：
   [
     {"name": "FUNC_NAME", "file": "FILE_PATH"},
     ...
   ]
   其中 file 可能是绝对路径，也可能是相对于 project_dir 的相对路径。

   逐一检查每个待处理条目（如从断点恢复，仅检查待处理条目）：

   Step 2a — 路径规范化：将 file 规范化为绝对路径。
   - 以 / 开头 → 绝对路径，直接使用
   - 不以 / 开头 → 相对路径，拼接为 <project_dir>/<相对路径>
   后续步骤均使用规范化后的绝对路径。记录 original_file = old_api.json 中的原始值。

   Step 2b — 路径范围检查：该接口规范化后的路径是否在 scope_files 中存在？
   ├── 在 scope_files 中 → 继续 Step 2c（函数存在性检查）
   └── 不在 scope_files 中 → 直接进入 Step 2d（fallback 搜索）

   Step 2c — 函数存在性检查：在 Step 2b 确定的 file 中，使用"搜索函数定义的方法"确认该函数定义存在。
   ├── 函数定义存在 → 继承。记录：
   │     result = "inherited", path_updated = false,
   │     reason = null
   └── 函数定义不存在 → 进入 Step 2d（fallback 搜索）

   Step 2d — fallback 搜索：
   检查 progress.results 中是否已有同名且 result = "inherited" 的条目？
   ├── 已有同名继承 → 淘汰。记录：
   │     result = "eliminated", path_updated = false,
   │     reason = "Step 2d：函数定义未在原路径对应的文件中找到，且同名函数 <name> 已被其他 old_api 条目继承（文件：<已有条目file>），跳过 fallback"
   │     处理下一条。
   └── 无同名继承 → 在 scope_files 中搜索该函数定义。
         使用"搜索函数定义的方法"在 <project_dir> 中搜索，并只保留文件路径在 scope_files 中的匹配结果。
         ├── 找到一个或多个 → 取第一个匹配的文件。记录：
         │     path_updated = true, file = 新找到的绝对路径,
         │     reason = "原路径对应的文件中未找到函数定义，在 scope_files 中找到同名函数定义于 <新路径>"
         │     result = "inherited"
         └── 未找到 → 淘汰。记录：
               result = "eliminated", path_updated = false,
               reason = "Step 2d：函数定义未在原路径对应的文件中找到，且 scope_files 中未找到同名函数定义"

   将本条处理记录追加到 progress.results，更新 progress.processed += 1。
   **每处理完一条立即保存 progress.json**，防止中断丢失进度。

3. 输出结果：
   - 将 progress.results 中 result = "inherited" 的条目（仅 name + file 字段，file 使用最终路径）写入
     <project_dir>/.ethunter_out/api-fixer/inherited_apis.json
   - 更新 progress.json：phase = "done"
```

本阶段仅做文件路径比对和函数名搜索，不涉及深度代码阅读。

---

## 四、约束规则

1. **所有分析由主 agent 完成，不使用 sub-agent 并行分析。** 不要调用 Agent 工具分派子任务。

2. **分析质量优先于分析效率。** 宁可慢不可草率。每个条目的源码必须认真阅读验证。

3. **严格遵循去重规则。** progress.results 中 name + original_file 唯一的条目仅处理一次。

4. **按设计流程的指定顺序执行。** 严格按照 2a → 2b → 2c → 2d 顺序，不跳过不合并。

5. **文件绝对路径。** inherited_apis.json 和 progress.json 中的 file 字段一律使用绝对路径。

6. **整个 skill 提示词用中文。** 相同语义的用词前后保持一致。

7. **适用业务：嵌入式/底层系统代码。** 适用于 Linux 内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU 固件。

8. **文件存在性检查严格遵循双方法+判断规则。** 禁止使用 `test -f` 或 `[ -f ]`，必须使用两种不同方法交叉确认。

---

## 五、权限申请

本 skill 运行时需要以下工具权限：

| 工具 | 用途 |
|------|------|
| `Bash` | find 枚举文件、grep 搜索代码、ls 检查文件存在、mkdir 创建输出目录 |
| `Read` | 读取源代码文件、读取 old_api.json、progress.json |
| `Write` | 写入 progress.json、inherited_apis.json |
| `mcp__codegraph__codegraph_explore` | (可选) codegraph 可用时优先使用的代码探索工具 |
| `mcp__plugin_oh-my-claudecode_t__ast_grep_search` | (可选) ast-grep 代码模式搜索 |

如果环境中未安装 codegraph 或未配置 MCP，不中断分析，使用常规工具（grep、find、Read）代替。

---

## 六、抗理性化检查

当你在分析过程中产生以下想法时，STOP：

| 想法 | 现实 |
|------|------|
| "条目太多了，我加快一点" | 分析质量优先于效率。每个条目必须认真验证。 |
| "这个函数名很常见，跳过 fallback 吧" | 必须完整执行 2b→2c→2d 流程。 |
| "这个函数可能已经删了，但不太确定，先保留吧" | 不确定时倾向淘汰。误报优先原则。 |
| "两步法太麻烦，我先看名字匹配就直接判定吧" | 必须通过 grep + Read 确认函数体定义存在，不能只看名称匹配。 |
| "这个状态文件检查结果不太确定，先继续吧" | 文件存在性检查必须严格遵循双方法+判断规则。 |

---

## 七、使用示例

### 首次分析

```
用户: /api-fixer /srv/workspace/work_code/src

Agent:
  [初始化] 分析范围: 127 个 .c/.h 文件。codegraph 未检测到，使用常规工具。
  [输入检查] old_api.json 确认存在（15 个历史接口）。
  [恢复检查] 未发现 progress.json，全新分析开始。

  [接口列表修复] 15 个待处理条目...
    → 条目 1/15: func_a — 继承（路径在范围且函数定义存在，路径未变）
    → 条目 2/15: func_b — 继承（路径不在范围，fallback 搜索到新路径，路径已更新）
    → 条目 3/15: func_c — 淘汰（函数定义未找到，scope_files 中也无同名函数）
    → 条目 4/15: func_d — 淘汰（同名函数已被条目 2 继承，跳过 fallback）
    → ...
    → 条目 15/15: func_o — 继承（路径在范围且函数定义存在）

  分析完成。
  总计: 15 个历史接口 | 继承: 12 | 淘汰: 3
  输出: /srv/workspace/work_code/src/.ethunter_out/api-fixer/inherited_apis.json
```

### 断点续分析

```
用户: /api-fixer /srv/workspace/work_code/src

Agent:
  [初始化] 分析范围: 127 个 .c/.h 文件。
  [输入检查] old_api.json 确认存在（15 个历史接口）。
  [恢复检查] 发现 progress.json，phase = "processing"，已处理 7/15，从条目 8 继续。

  [接口列表修复] 剩余 8 个条目...
    → 条目 8/15: func_h — 继承
    → ...
    → 条目 15/15: func_o — 继承

  分析完成。继承: 12 | 淘汰: 3
```
