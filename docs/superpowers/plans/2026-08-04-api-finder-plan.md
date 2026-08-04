# api-finder Skill 实现方案

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建 api-finder skill，一个纯 markdown 的 SKILL.md 文件，用于在嵌入式/底层 C 项目中识别对外部暴露的接口函数。

**Architecture:** 单个 SKILL.md 文件，内部按分析流水线组织：初始化 → 架构分析 → 接口继承 → 特征提取(两子阶段) → 架构识别(分批) → 筛选 → 输出。所有分析由主 agent 完成，通过 progress.json + 各阶段专属状态文件实现断点续跑。

**Tech Stack:** 纯 markdown skill，运行时依赖 Bash(grep/find/ls)、Read、Write 工具 + 可选的 codegraph MCP 工具。

---

## 文件结构

| 文件 | 职责 |
|------|------|
| `api-finder/SKILL.md` (创建) | skill 全部内容，包含 frontmatter、工作流、各阶段详细指令 |

所有内容在一个文件中，按 spec 的顺序组织。各阶段之间通过 JSON 中间文件传递数据。

---

### Task 1: 创建 SKILL.md 骨架与 frontmatter

**Files:**
- Create: `api-finder/SKILL.md`

- [ ] **Step 1: 创建目录并写入 SKILL.md 头部（frontmatter + 核心原则 + 使用说明）**

```bash
mkdir -p /home/admin/cc/wksp/siakam_security_skills/api_finder/api-finder
```

写入 `/home/admin/cc/wksp/siakam_security_skills/api_finder/api-finder/SKILL.md`：

```markdown
---
name: api-finder
description: 识别嵌入式/底层C项目中对外暴露的接口函数。用于安全审计，聚焦外部不可信输入的攻击面分析。适用于Linux内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU固件。使用方式：/api-finder <project_dir>
---

# api-finder

在嵌入式/底层C项目中识别"外部接口"——本项目模块定义的、供外部用户或外部模块与本模块通信交互的函数。重点关注向本模块传入外部（不可信）数据的接口，用于安全攻击面分析。

## 核心原则

1. **分析质量优先于分析效率。** 宁可慢，不可草率。每批文件必须完整深入分析，不得因批次多而加速跳读。每个文件都要认真阅读和理解。

2. **更少的误报优先于更少的漏报。** 不确定是否为外部接口时，倾向于排除而非保留。只在有充分证据（代码模式 + 架构信息双重支撑）时才认定为外部接口。证据不足的函数：宁可漏排，不可误报。

## 使用方法

```
/api-finder <project_dir>
```

如果未指定 `project_dir`，默认为当前工作目录。

## 输出

分析完成后在 `<project_dir>/.ethunter_out/api-finder/` 下生成：
- `api.json` — 识别出的外部接口列表
- `summary.md` — 逐条识别理由说明
- `arch.md` — 项目架构分析（中间产物）
- `progress.json` — 断点续分析状态
- `tmp/` — 各阶段中间结果和状态文件
```

- [ ] **Step 2: 验证文件创建成功**

```bash
ls -la /home/admin/cc/wksp/siakam_security_skills/api_finder/api-finder/SKILL.md
```

Expected: 文件存在，非空。

- [ ] **Step 3: Commit**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/api_finder && git add api-finder/SKILL.md && git commit -m "feat(api-finder): add SKILL.md skeleton with frontmatter and core principles"
```

---

### Task 2: 写入分析范围确定 + 任务恢复机制

**Files:**
- Modify: `api-finder/SKILL.md` (追加)

- [ ] **Step 1: 追加"分析范围确定"和"任务恢复机制"两部分**

在 SKILL.md 末尾追加：

```markdown
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

确认存在后，读取 clean_code.txt，每一行是一个文件的绝对路径，将这些文件作为分析范围（scope_files）。注意：先记住 clean_code.txt 的内容，再去重——可能包含重复路径。

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

### Step 3: 确认 macro.json 信息

用两种方法检查 `<project_dir>/.ethunter_out/macro.json` 是否存在。如果存在，LLM 了解该文件记录了编译选项中的宏定义即可（注意：这不是全部宏，仅编译指令中通过 -D 等选项动态定义的宏。还有更多的宏在代码中定义，不在 macro.json 的覆盖范围内）。

**不需要预先读取 macro.json。** 在后续分析中，当遇到 `#ifdef` / `#if` 控制的代码分支，且需要判断宏的值来决定分析范围时，除了在代码中追溯宏定义，也到 macro.json 中查找。如果某个宏在代码和 macro.json 中都找不到定义，**默认进行分析**（不排除该分支代码）。

### Step 4: 检测 CodeGraph 可用性

用两种方法检查 `<project_dir>/.codegraph` 目录是否存在。如果存在，在后续所有代码分析中**优先使用 MCP 工具 `codegraph_explore`** 进行代码搜索和理解。如果不存在、或环境中未安装 codegraph 或未配置 MCP，使用常规工具（grep、find、Read、ast-grep），不中断分析。

### Step 5: 初始化输出目录

```bash
mkdir -p <project_dir>/.ethunter_out/api-finder/conf
mkdir -p <project_dir>/.ethunter_out/api-finder/tmp
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
| 两次都无输出或都报错 | **立即停止分析，向用户报告问题**。不要假设文件缺失后继续分析 |

### 入口恢复流程

在处理任何分析步骤之前，首先执行恢复检查：

```
1. 用两种方法检查 <project_dir>/.ethunter_out/api-finder/progress.json 是否存在。
   根据判断规则：
   ├── 确认不存在 → 全新分析，从"三、了解项目架构"开始
   └── 确认存在 → 读取 progress.json，找到当前 phase 字段的值

2. 如果 phase 指向复杂阶段（feature / arch_identify / filter）：
   用两种方法检查对应 state_file 指向的文件是否存在：
   ├── 确认存在 → 加载状态文件，从断点处继续该阶段
   └── 确认不存在（明确报错）→ 状态文件丢失，该阶段退回重做
       （将 progress.json 中该阶段的 status 改为 pending，phase 改回该阶段）

3. 如果 phase = "done"：分析已完成，告知用户并停止。用户可以手动删除 progress.json 后重新分析。
```

### progress.json 结构

```json
{
  "phase": "arch_analysis|inherit|feature|arch_identify|filter|done",
  "arch_analysis": { "status": "pending|in_progress|completed" },
  "inherit": { "status": "pending|in_progress|completed" },
  "feature": {
    "status": "pending|in_progress|completed",
    "state_file": "tmp/feature_state.json"
  },
  "arch_identify": {
    "status": "pending|in_progress|completed",
    "state_file": "tmp/arch_identify_state.json"
  },
  "filter": {
    "status": "pending|in_progress|completed",
    "state_file": "tmp/filter_state.json"
  }
}
```

每次开始新的分析阶段时，更新对应阶段的 status 为 `in_progress`，完成时更新为 `completed` 并更新 `phase` 为下一个阶段。
```

- [ ] **Step 2: Commit**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/api_finder && git add api-finder/SKILL.md && git commit -m "feat(api-finder): add scope determination and resume mechanism"
```

---

### Task 3: 写入了解项目架构 + 接口继承

**Files:**
- Modify: `api-finder/SKILL.md` (追加)

- [ ] **Step 1: 追加"了解项目架构"和"接口继承"两阶段**

在 SKILL.md 末尾追加：

```markdown
---

## 三、了解项目架构（arch_analysis）

本阶段是后续所有分析的基石。arch.md 的质量直接决定最终识别效果。

### 进入检查

```
1. 用两种方法检查 <project_dir>/.ethunter_out/api-finder/arch.md 是否存在。
   根据判断规则：
   ├── 确认存在 → 跳过本阶段，更新 progress.json 中 arch_analysis.status = "completed"，
   │     phase = "inherit"，继续下一阶段
   └── 确认不存在 → 执行架构分析
```

### 分析执行

更新 progress.json：设置 `arch_analysis.status = "in_progress"`，`phase = "arch_analysis"`。

1. **浏览关键文件：** 优先从 scope_files 中选取头文件（`.h`）开始阅读，了解模块对外暴露的类型定义和函数声明。然后选取几个核心的 `.c` 文件（优先选择 main 文件、init 文件、或文件名包含 `main`/`init`/`core`/`drv` 关键词的文件），了解模块的实现逻辑。

2. **分析三个维度：**

   - **模块概要：** 本模块的功能定位是什么？在系统中扮演什么角色？是内核驱动、UEFI固件、XLoader、还是协处理器固件（ISP/SensorHub/GPU）？主要职责有哪些？结合目录名、文件名、代码注释和函数命名惯例进行推断。

   - **外部通信边界：** 本模块与哪些外部实体有通信？通信方向是什么（外部→本模块、本模块→外部、双向）？通信信道是什么（系统调用/ioctl、共享内存、IPC消息队列、网络socket、硬件寄存器/MMIO、DMA缓冲区、GPIO/中断、文件系统）？结合代码中出现的通信相关 API（如 `copy_from_user`、`readl`/`writel`、`shm_open`、`msgsnd`/`msgrcv`、`recv`/`send`、`read`/`write` 等）进行推断。

   - **代码分区映射：** 按功能或通信方向将 scope_files 中的文件划分为不同的组。标注哪些文件最有可能包含外部接口（核心通信文件），哪些是内部实现（内部工具、配置管理、日志等，但也需要覆盖，因为可能包含间接的外部通信点）。

3. **生成任务文件列表：** 从代码分区映射中整理出一个完整的待分析文件列表（必须在 scope_files 范围内），按优先级排序：核心通信文件在前，内部实现文件在后。该列表将作为"五、架构识别接口"的任务清单。

4. **输出 arch.md：** 按照以下模板写入 `<project_dir>/.ethunter_out/api-finder/arch.md`：

```markdown
# 项目架构分析 — <project_name>

## 模块概要
本项目模块是 <功能定位>，在系统中扮演 <角色> 角色。
主要职责包括：<列举关键职责>。

## 外部通信边界
| 外部实体 | 通信方向 | 通信信道 | 说明 |
|---------|---------|---------|------|
| <实体名> | 外部→本模块 | <信道名> | <具体说明> |

## 代码分区映射

### 核心通信文件（待深入分析，优先级高）
- <文件绝对路径> — <说明，为何认为该文件包含外部接口>
- <文件绝对路径> — <说明>

### 内部实现文件（优先级低，但也需覆盖）
- <文件绝对路径> — <说明>
```

5. 更新 progress.json：`arch_analysis.status = "completed"`，`phase = "inherit"`。

---

## 四、接口继承（inherit）

将历史版本中已识别的外部接口与当前分析范围做交叉比对。

### 流程

```
1. 用两种方法检查 <project_dir>/.ethunter_out/api-finder/conf/old_api.json 是否存在。
   根据判断规则：
   ├── 确认不存在 → 写入空的 inherited_apis.json（内容为 []），
   │     更新 progress.json 中 inherit.status = "completed"，phase = "feature"，继续下一阶段
   └── 确认存在 → 继续

2. 读取 old_api.json。其格式为：
   [
     {"name": "FUNC_NAME", "file": "FILE_PATH"},
     ...
   ]
   逐一检查每个条目：该接口的 file 字段是否在 scope_files 中存在？
   ├── 不在 scope_files 中 → 排除（文件可能已被删除或不在本次分析范围）
   └── 在 scope_files 中 → 计入继承接口列表

3. 将继承接口列表写入 <project_dir>/.ethunter_out/api-finder/tmp/inherited_apis.json，
   格式与 old_api.json 一致。

4. 更新 progress.json：inherit.status = "completed"，phase = "feature"。
```

**重点注意：** 本阶段不涉及代码阅读，仅做文件路径级别的比对。不输出的情况：old_api.json 不存在时，写入空的 `[]` 即可，这是正常情况（首次分析或历史数据已清理）。
```

- [ ] **Step 2: Commit**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/api_finder && git add api-finder/SKILL.md && git commit -m "feat(api-finder): add arch analysis and inherit phases"
```

---

### Task 4: 写入接口特征提取（feature 两子阶段）

**Files:**
- Modify: `api-finder/SKILL.md` (追加)

- [ ] **Step 1: 追加"接口特征提取"阶段**

在 SKILL.md 末尾追加：

```markdown
---

## 五、接口特征提取（feature）

复杂阶段。从继承接口中提取注册特征，在全代码范围内推广匹配。

### 前置条件检查

```
读取 <project_dir>/.ethunter_out/api-finder/tmp/inherited_apis.json
├── 内容为 []（空数组）→ 跳过本阶段。
│     更新 progress.json：feature.status = "completed"，phase = "arch_identify"
│     继续下一阶段
└── 内容不为空 → 进入本阶段
```

### 进入检查

```
1. 用两种方法检查 <project_dir>/.ethunter_out/api-finder/tmp/feature_state.json 是否存在。
   根据判断规则：
   ├── 确认存在 → 从断点恢复，跳转到 feature_state.json 中 sub_phase 对应的子阶段继续
   └── 确认不存在 → 全新启动，初始化 feature_state.json（见下方结构）
```

### feature_state.json 结构

```json
{
  "sub_phase": "collect|match",
  "seeds": {
    "total": <N>,
    "current": 1,
    "done": [],
    "remaining": [1, 2, ..., <N>]
  },
  "patterns": [],
  "candidate_apis": []
}
```

初始化时：
- `sub_phase` = `"collect"`
- 读取 inherited_apis.json，令 `seeds.total` = 条目数
- `seeds.current` = 1
- `seeds.remaining` = `[1, 2, ..., total]`（1-based 序号）
- `seeds.done` = `[]`
- `patterns` = `[]`
- `candidate_apis` = `[]`

更新 progress.json：`feature.status = "in_progress"`，`phase = "feature"`。

---

### 子阶段一：收集特征（collect）

升级恢复时，从 `seeds.current` 指向的种子继续；`seeds.done` 中的种子已完成，跳到下一个 `remaining` 中的种子。

**对当前种子执行以下分析：**

1. 读取 `inherited_apis.json` 中第 `seeds.current` 个条目，获取函数名（记为 `$FUNC`）和文件路径。

2. 找到 `$FUNC` 在代码中的**全部使用点**（优先使用 `codegraph_explore` 查询函数引用，如果 codegraph 不可用，则用 `grep -rn "$FUNC" <project_dir>` 在所有 scope_files 中搜索）。

3. **排查每个使用点**，判断是否为注册点：

   读取使用点所在文件的上下文代码（使用点前后 20 行），判断：

   | 使用形式 | 判断 | 处理 |
   |---------|------|------|
   | 正常的函数调用 `func(args)` | 不是注册点 | 跳过 |
   | 作为参数传递给另一个函数 `register(func)` 或 `register(&func)` | **是注册点（动态注册）** | 继续追踪 |
   | 赋值给结构体字段 `.field = func` 或数组元素 `{CMD, func}` | **是注册点（静态注册）** | 继续追踪 |
   | 取地址 `&func` 并赋值给某变量/字段 | **是注册点** | 继续追踪 |
   | 函数声明、头文件声明 | 不是注册点 | 跳过 |
   | 注释中提及、文档引用 | 不是注册点 | 跳过 |

4. **确认注册点后，继续追踪**：

   - **静态注册**：追踪包含该注册点的全局变量/数组的**类型定义**。例如 `{CMD, func}` 被填入数组 `table[]`，找出 `table` 的声明，获取其类型名（如 `ap_msg_handle_func`）。记录：变量类型名、变量名、被赋值的是哪个字段。

   - **动态注册**：追踪注册函数的**实现代码**（使用 `codegraph_explore` 或 `grep` 查找注册函数的定义，阅读其完整代码），理解被注册的函数参数在注册函数内部被如何存储（赋值给哪个结构体的哪个字段）。记录：注册函数名、被注册函数的参数位置（第几个参数）。

5. 将提取的特征添加到 `patterns` 列表：

```json
{
  "id": <自动递增编号>,
  "status": "pending",
  "summary": "<一句话描述该注册模式>",
  "source_seed": "<当前种子函数名>",
  "details": {
    "type": "static|dynamic",
    ...<根据类型填写具体字段>
  }
}
```

**静态注册的 details 示例：**
```json
{
  "type": "static",
  "var_type": "<全局变量类型名>",
  "var_name": "<全局变量名>",
  "binding_field": "<被绑定的字段名>"
}
```

**动态注册的 details 示例：**
```json
{
  "type": "dynamic",
  "register_func": "<注册函数名>",
  "callback_param_index": <参数位置(1-based)>
}
```

details 字段可扩展。当遇到类型不匹配的注册模式（如间接注册、多层包装、宏注册等），自行新增 type 名称并填写对应字段，不破坏已有 pattern 结构。扩展规则：`type`、`summary`、`source_seed` 必填，`details` 内字段自由但需自描述。

6. 更新 feature_state.json：
   - 将 `seeds.current` 从 `seeds.remaining` 移动到 `seeds.done`
   - 如果 `seeds.remaining` 不为空，取下一个作为 `seeds.current`，重复步骤 1-6
   - 如果 `seeds.remaining` 为空，种子分析完成 → 进入去重步骤

7. **收集阶段完成后，进入去重步骤**：

   读取当前 `patterns` 列表。两个或多个种子函数如果实际上绑定到了**同一个 handler 机制的不同实例**（例如同一个全局数组 `table` 的第 0 项和第 1 项），它们应合并为一个 pattern。

   去重时需要深度理解代码语义：
   - 比较每对 pattern 的 details 字段
   - 两个 pattern 同类型（都是 static 或都是 dynamic）且共享相同的注册载体（同一个 `var_type`+`var_name` 或同一个 `register_func`）→ 合并
   - 合并后更新 summary，说明该模式来自多个种子接口

   去重后为每个 pattern 重新分配独立编号（从 1 开始）。

   **如果经过分析，没有任何注册特征被发现，patterns 保持为空数组**（这是可能的，例如种子接口都是通过架构语义识别而非注册模式命中的）。

8. 将 `sub_phase` 更新为 `"match"`，保存 feature_state.json。

---

### 子阶段二：推广匹配（match）

升级恢复时，从 `patterns` 中 `status = "pending"` 的第一个 pattern 继续。

**对每个 pending pattern 执行以下分析：**

1. 修改该 pattern 的 `status` 为 `"analyzed"`，保存 state。

2. 根据 pattern 的类型在全代码范围（scope_files）内搜索：

   - **静态注册**：搜索与该 pattern 中 `var_type` 类型相同的全局变量或数组的定义。使用 `grep -rn "<var_type>" <project_dir>` 找到使用该类型的声明，然后查看定义中绑定了哪些函数。对于数组定义中绑定的多个函数，提取全部。

   - **动态注册**：搜索注册函数的所有调用点。使用 `grep -rn "<register_func>(" <project_dir>` 找到所有调用该注册函数的位置，提取每个调用点中被注册的函数（即 `callback_param_index` 位置的参数）。

3. 对找到的每个函数（作为候选接口）：
   - 如果该函数已在 inherited_apis.json 中 → 跳过，不计入新发现
   - 结合 arch.md 的"外部通信边界"信息做交叉验证：
     - 该函数是否能与某个通信边界关联？
     - 函数功能是否对应某个外部实体的通信需求？
   - **无法与任何通信边界关联 → 排除**（误报优先原则）
   - 能与通信边界关联 → 加入 `candidate_apis`

   candidate_apis 条目格式：
   ```json
   {"name": "<函数名>", "file": "<文件绝对路径>", "matched_pattern_id": <pattern.id>}
   ```

4. 继续处理下一个 pending pattern，直到全部 patterns 的 status 都是 `"analyzed"`。如果没有 patterns（patterns 为空数组），直接完成本阶段。

---

### 输出

将 `candidate_apis` 的内容写入 `<project_dir>/.ethunter_out/api-finder/tmp/feature_apis.json`。

更新 progress.json：`feature.status = "completed"`，`phase = "arch_identify"`。
```

- [ ] **Step 2: Commit**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/api_finder && git add api-finder/SKILL.md && git commit -m "feat(api-finder): add feature extraction phase with two sub-phases"
```

---

### Task 5: 写入架构识别接口（arch_identify）分批分析

**Files:**
- Modify: `api-finder/SKILL.md` (追加)

- [ ] **Step 1: 追加"架构识别接口"阶段**

在 SKILL.md 末尾追加：

```markdown
---

## 六、架构识别接口（arch_identify）

最核心的复杂阶段。根据 arch.md 的架构信息，逐文件深入分析，识别两种形式的外部接口。

**本阶段永不跳过。** 即使无继承接口、无特征匹配，架构识别也必须执行。

### 进入检查

```
1. 用两种方法检查 <project_dir>/.ethunter_out/api-finder/tmp/arch_identify_state.json 是否存在。
   根据判断规则：
   ├── 确认存在 → 加载状态，从中断位置恢复（跳到"分批分析循环"）
   └── 确认不存在 → 全新启动

2. 更新 progress.json：arch_identify.status = "in_progress"，phase = "arch_identify"
```

### 全新启动 — 准备任务列表

1. 读取 `arch.md` 的"代码分区映射"部分，提取全部文件路径（核心通信文件 + 内部实现文件），组成 `file_list`。

2. 按下列优先级排序（如果 arch.md 中已有优先级标注，直接使用）：
   - 核心通信文件（优先级高）→ 在前
   - 内部实现文件（优先级低）→ 在后

3. **每 10 个文件为一批**，生成全部 batches：

```json
{
  "file_list": ["<文件1>", "<文件2>", ...],
  "total_files": <文件总数>,
  "batch_size": 10,
  "batches": [
    {"index": 1, "files": ["<文件1>", ..., "<文件10>"], "status": "pending"},
    {"index": 2, "files": ["<文件11>", ..., "<文件20>"], "status": "pending"},
    ...
  ],
  "batch_current": 1,
  "found_apis": []
}
```

4. 保存 `<project_dir>/.ethunter_out/api-finder/tmp/arch_identify_state.json`。

### 分批分析循环

从 `batch_current`（值为当前批次号）对应的批次开始。

**对当前批次的每个文件，执行以下分析：**

1. 读取文件完整内容。如果文件很大（超过 2000 行），分批读取。

2. 结合 arch.md 的"外部通信边界"信息，寻找两种形式的外部接口。

#### 形式一：外部输入来自参数（parameter_input）

识别特征：
- 函数被注册为 handler/回调，供外部模块调用
- 函数自身不会被本项目内部代码直接调用（或仅在注册点被引用，不在注册点之外被调用）
- 函数的参数来自外部不可信源

分析方法：
```
a. 在文件中逐个查看函数定义。对每个函数：
   - 是否有参数？如果没有，标记其可能为形式二候选。
   - 有参数的话，参数的类型是什么？是否是指针类型（void*、char*、自定义结构体指针等）？
     通常外部输入以指针形式传入。

b. 追踪函数在本项目中的调用关系：
   - 优先用 codegraph_explore 查询该函数的引用点
   - 如果没有 codegraph，用 grep -rn "函数名" 搜索全部 scope_files
   - 检查每个引用点：是函数调用，还是作为 handler 赋值/注册？
   - 如果所有引用都是注册点（赋值给 handler/作为注册参数）+ 没有普通函数调用 → 更有可能是外部接口

c. 参考 arch.md 的通信边界信息：
   - 该文件属于哪个通信分区？与哪个外部实体通信？
   - 函数的参数是否与该通信信道的数据格式匹配？
```

#### 形式二：外部输入来自信道读取（channel_read）

识别特征：
- 函数体内调用了从共享内存、文件、IPC 信道等读取数据的操作
- 从这些信道获取的数据来自外部模块/外部世界

分析方法：
```
a. 查看函数体内调用了哪些数据读取函数。寻找以下模式：
   - 共享内存读取：shm_open、shmget、shmat、mmap + 读取、read_from_shared_memory 等
   - 寄存器/MMIO读取：readl、readw、readb、ioread32、MMIO_READ 等
   - IPC消息接收：msgrcv、mq_receive、recv、recvfrom、recvmsg 等
   - 文件读取：read、fread、pread、aio_read 等
   - DMA缓冲区读取：dma_alloc_coherent + 读取、mmap dma buffer 等
   - 网络socket读取：recv、recvfrom、read from socket fd 等
   - 管道/FIFO读取：read from pipe fd 等

b. 确认读取的数据来源：
   - 读取的数据来自何处？是本模块内部写入的，还是外部实体写入的？
   - 参考 arch.md 通信边界信息进行判断。
   - 如果是内部函数间传递的缓冲区 → 排除
   - 如果是外部实体写入的信道 → 保留
```

3. **对找到的每个候选接口，执行 LLM 深度验证：**

   回答以下问题并记录：
   - 该函数的功能是什么？（简明总结）
   - 与哪个外部模块/实体通信？（参考 arch.md）
   - 输入数据的来源是项目内部还是外部？
   - 有没有可能这是一个内部工具函数？

   **验证结论：**
   - 能与 arch.md 中的通信边界明确关联 → 保留为外部接口
   - 内部函数间的数据传递（参数来自本项目其他函数计算的结果）→ 排除
   - 无法与任何通信边界关联、无法判断 → **排除**（误报优先原则）

4. 记录发现到 found_apis：

```json
{"name": "<函数名>", "file": "<文件绝对路径>", "form": "parameter_input|channel_read", "reason": "<验证结论摘要>"}
```

**注意：** 不记录已在 inherited_apis.json 或 feature_apis.json 中的接口（通过 name + file 去重）。

**批次内对每个文件都执行以上分析流程，即使某文件不含外部接口也不跳过。不含外部接口的文件不做记录即可。**

### 批次完成

当前批次全部文件分析完毕：

1. 更新 arch_identify_state.json：
   - 将 `batches` 中当前批次（index = batch_current）的 status 改为 `"completed"`
   - 将 `batch_current` 改为下一个 pending 批次的 index
   - 如果没有更多 pending 批次 → 批次循环结束

2. 保存 arch_identify_state.json。

### 关键规则

- 分析必须覆盖 `file_list` 中**全部文件**，不提前退出
- 判断"是否为外部输入"时存疑则排除；仅明确的外部通信入口才保留
- 每次读取代码时注意不要遗漏，读大文件时记得分批读取（每次 500-2000 行）

### 阶段完成

全部批次完成后：

1. 将 `found_apis` 写入 `<project_dir>/.ethunter_out/api-finder/tmp/arch_apis.json`
2. 更新 progress.json：`arch_identify.status = "completed"`，`phase = "filter"`
```

- [ ] **Step 2: Commit**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/api_finder && git add api-finder/SKILL.md && git commit -m "feat(api-finder): add arch identify phase with batched file analysis"
```

---

### Task 6: 写入外部接口筛选 + 最终输出

**Files:**
- Modify: `api-finder/SKILL.md` (追加)

- [ ] **Step 1: 追加"外部接口筛选"和"最终输出"两阶段**

在 SKILL.md 末尾追加：

```markdown
---

## 七、外部接口筛选（filter）

合并前三阶段的接口列表，逐条审查，排除不符合条件的接口。

### 合并接口列表

```
1. 读取以下文件（如果某文件为空数组或不存在，跳过该文件）：
   - tmp/inherited_apis.json
   - tmp/feature_apis.json
   - tmp/arch_apis.json

2. 合并所有条目，按 name + file 去重（两个条目函数名和文件路径都一致视为重复，保留一个即可）。
   去重后的列表称为 api_pool。
```

### 黑名单筛选

```
用两种方法检查 <project_dir>/.ethunter_out/api-finder/conf/black_api.json 是否存在。
├── 确认存在 → 读取 black_api.json。
│     将 api_pool 中 name + file 与黑名单条目完全匹配的接口排除。
│     排除的接口不进入后续审查。
└── 确认不存在 → 跳过黑名单筛选
```

### 进入检查

```
1. 用两种方法检查 <project_dir>/.ethunter_out/api-finder/tmp/filter_state.json 是否存在。
   根据判断规则：
   ├── 确认存在 → 加载 filter_state.json，跳过 reviewed: true 的项
   └── 确认不存在 → 全新启动，初始化 filter_state.json
```

### filter_state.json 结构

```json
{
  "api_total": <api_pool条目数>,
  "api_list": [
    {"name": "<函数名>", "file": "<文件路径>", "reviewed": false, "decision": null, "reason": null},
    ...
  ]
}
```

全新启动时，将 api_pool 中全部条目写入 api_list，清空 decision/reason。

更新 progress.json：`filter.status = "in_progress"`，`phase = "filter"`。

### 逐条审查循环

对 api_list 中每个 `reviewed = false` 的条目：

1. 读取函数所在文件，定位到函数定义处，阅读完整函数代码。

   **如果该函数在之前的分析阶段（arch_identify 或 feature）已经充分阅读过且你对其有清晰记忆，可以跳过重复阅读。**

2. 按照以下三条规则逐一判断保留/排除：

#### 规则一：测试函数排除

**建议规则（不是硬性规定）：**
- 函数名包含以下前缀或后缀：`test_`、`_test`、`mock_`、`stub_`、`demo_`、`sample_`
- 函数所在文件位于 `test/`、`tests/`、`unittest/`、`mock/` 目录下

**LLM 可根据上下文灵活判断：** 即使不完全匹配以上规则，但根据代码注释标记为"测试用"、"for test only"、"调试用"，或函数仅在测试/调试代码中被引用，也可以判断为测试函数并排除。

#### 规则二：无外部输入排除（核心规则）

**核心判断：函数是否接收来自外部模块/外部世界的数据？**

判断逻辑：

```
函数是否有入参？
├── 有入参
│   ├── 参数是否全部为系统框架类参数？（如 struct file *f、struct inode *i、
│   │   内核内部数据结构的指针、框架回调约定的上下文参数等）
│   │   → 这些都是内部框架参数，不是外部输入 → 排除
│   │
│   ├── 参数是否用于输出目的？（如 void *out_buffer、int *result）
│   │   → 根据代码语义判断，输出参数不携带外部输入 → 排除
│   │
│   └── 是否存在至少一个参数携带外部输入数据？
│       → 是 → 保留。否 → 排除
│
└── 无入参
    └── 函数体是否从共享内存/文件/IPC/寄存器等信道获取外部数据？
        → 是 → 保留。否 → 排除
```

**重要：** 判断时仔细分析每个参数的实际用途。不要看到有参数就认为有外部输入。
例如：内核驱动中 `struct file *filp` 是内核框架传入的上下文指针，不携带用户数据。
例如：`void *output_buffer` 是用于往外部写的缓冲区，不是外部输入。

#### 规则三：冗余函数排除

**建议规则（不是硬性规定）：**
- 函数体为空（只包含 `return;` 或不含任何有效代码）
- 函数仅调用另一个完全相同签名的函数（纯转发/包装）
- 函数被注释标记为 `deprecated`、`unused`、`不再使用`
- 函数仅包含日志打印或调试输出语句，无实际数据处理逻辑

**LLM 可根据代码语义灵活判断。**

3. **规则判断边界模糊时 → 倾向排除**（误报优先原则）。

4. **reason 字段必须写明确凿证据**，不使用模糊描述：
   - 好的 reason：`"data参数是用户态通过ioctl传入的缓冲区指针，来自copy_from_user填充后的内存"`
   - 不好的 reason：`"看起来像外部接口"`、`"可能接收外部数据"`

5. 更新条目状态：
```json
{"name": "<函数名>", "file": "<文件路径>", "reviewed": true, "decision": "keep|exclude", "reason": "<具体理由>"}
```

6. 保存 filter_state.json（每审查完一条就保存一次，防止中断丢失进度）。

7. 继续下一条 `reviewed = false` 的条目，直到全部审查完毕。

### 输出结果

从 api_list 中提取所有 `decision = "keep"` 的条目，按原始顺序写入：

1. **api.json** — `<project_dir>/.ethunter_out/api-finder/api.json`：
```json
[
  {"name": "<函数名>", "file": "<文件绝对路径>"},
  ...
]
```
格式与 old_api.json/black_api.json 一致。

2. **summary.md** — `<project_dir>/.ethunter_out/api-finder/summary.md`：

按 api.json 的顺序，逐条说明，每条包含以下信息：

```markdown
# 外部接口识别报告 — <project_dir>

## <序号>. <函数名> — <文件路径>

- **识别路径：** <继承 / 特征识别 / 架构识别>（可能有多个路径命中同一接口，全部列出）
- **通信边界：** <该接口与哪个外部模块通信，通过什么信道>
- **外部输入：** <接收什么外部输入，来自哪里>
- **保留原因：** <筛选阶段的保留理由>
```

如果某个接口被多条识别路径命中（例如既是继承接口又被架构识别再次发现），在识别路径中列出全部来源。

3. 更新 progress.json：`filter.status = "completed"`，`phase = "done"`。

### 分析完成

告知用户分析完成，报告关键数字：
- 分析范围的文件数量
- 识别出的外部接口数量
- 输出文件路径：`<project_dir>/.ethunter_out/api-finder/api.json` 和 `summary.md`
```

- [ ] **Step 2: Commit**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/api_finder && git add api-finder/SKILL.md && git commit -m "feat(api-finder): add filter phase and final output"
```

---

### Task 7: 追加权限申请 + 约束规则 + 使用示例

**Files:**
- Modify: `api-finder/SKILL.md` (追加)

- [ ] **Step 1: 追加权限、约束规则和示例**

在 SKILL.md 末尾追加：

```markdown
---

## 八、权限申请

本 skill 运行时需要以下工具权限：

| 工具 | 用途 |
|------|------|
| `Bash` | find 枚举文件、grep 搜索代码、ls 检查文件存在、mkdir 创建输出目录 |
| `Read` | 读取源代码文件、读取配置/状态 JSON 文件 |
| `Write` | 写入 arch.md、progress.json、api.json、summary.md 和各阶段中间/状态文件 |
| `mcp__codegraph__codegraph_explore` | (可选) codegraph 可用时优先使用的代码探索工具 |
| `mcp__plugin_oh-my-claudecode_t__ast_grep_search` | (可选) ast-grep 代码模式搜索 |

如果环境中未安装 codegraph 或未配置 MCP，不中断分析，使用常规工具（grep、find、Read）代替。

---

## 九、约束规则

1. **所有分析由主 agent 完成，不使用 sub-agent 并行分析。** 不要调用 Agent 工具分派子任务。

2. **分析质量优先于分析效率。** 宁可慢不可草率。每批文件（10 个）逐一认真阅读，不因批次多而加速跳读。如果文件很大，分批读取（每次 500-2000 行）。

3. **代码溯源要深入。** 判断注册特征、分析函数用途时，需要追踪代码定义和调用关系，不要只看表面名称和注释就下结论。

4. **严格遵循去重规则。** 每个阶段输出的接口列表不要与之前阶段的结果重复（以 name + file 为准）。

5. **按设计流程的指定顺序执行。** 不要跳过或合并阶段。遇到空列表或数据缺失时，按各阶段的前置条件处理（跳过或终止），仍继续后续阶段。

6. **文件绝对路径。** api.json、中间结果 JSON 中的 file 字段一律使用绝对路径。

---

## 十、使用示例

```
用户: /api-finder /srv/workspace/work_code/src

Agent:
  [初始化] 分析范围: 127 个 .c/.h 文件。macro.json 已发现（按需查阅）。
  [恢复检查] 未发现 progress.json，全新分析开始。
  [阶段1/6] 了解项目架构... → arch.md 已生成（3 个外部通信边界，48 个待分析文件）
  [阶段2/6] 接口继承... → 未发现 old_api.json，跳过继承
  [阶段3/6] 接口特征提取... → inherited_apis 为空，跳过特征提取
  [阶段4/6] 架构识别接口... → 正在分析批次 1/5 (文件 1-10)...
  [阶段4/6] 批次 1/5 完成，发现 3 个候选接口
  [阶段4/6] 批次 2/5 (文件 11-20)... 发现 2 个候选接口
  ...
  [阶段5/6] 外部接口筛选... → 合并去重后 18 个候选，逐条审查中...
  [阶段5/6] 筛选完成: 保留 12 个，排除 6 个
  [阶段6/6] 输出结果... → api.json + summary.md 已生成

  分析完成。
  范围: 127 个文件
  外部接口: 12 个
  输出: /srv/workspace/work_code/src/.ethunter_out/api-finder/api.json
        /srv/workspace/work_code/src/.ethunter_out/api-finder/summary.md
```

### 断点续分析示例

```
用户: /api-finder /srv/workspace/work_code/src

Agent:
  [初始化] 分析范围: 127 个 .c/.h 文件。
  [恢复检查] 发现 progress.json，phase = "arch_identify"
  [恢复检查] arch_identify_state.json 确认存在，批次 3/5 未完成
  [阶段4/6] 从批次 3 (文件 21-30) 继续...
  ...
```

### 迭代分析示例（第二次分析，有 old_api.json）

```
用户: /api-finder /srv/workspace/work_code/src

Agent:
  [初始化] 分析范围: 130 个 .c/.h 文件（新增 3 个文件）。
  [恢复检查] 未发现 progress.json，全新分析开始。
  [阶段1/6] 了解项目架构... → arch.md 已生成
  [阶段2/6] 接口继承... → old_api.json 中发现 12 个历史接口，10 个仍在范围内 → 继承 10 个
  [阶段3/6] 接口特征提取... → 从 10 个种子提取 3 个注册模式，推广匹配发现 4 个新接口
  [阶段4/6] 架构识别接口... → 分批分析，发现 5 个候选接口
  [阶段5/6] 外部接口筛选... → 合并去重后 19 个候选，保留 14 个
  [阶段6/6] 输出结果... → api.json + summary.md 已生成

  分析完成。
  范围: 130 个文件
  外部接口: 14 个（其中 10 个继承，4 个新发现）
  输出: /srv/workspace/work_code/src/.ethunter_out/api-finder/api.json
```
```

- [ ] **Step 2: 验证完整 SKILL.md 文件**

```bash
wc -l /home/admin/cc/wksp/siakam_security_skills/api_finder/api-finder/SKILL.md
```

- [ ] **Step 3: Commit**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/api_finder && git add api-finder/SKILL.md && git commit -m "feat(api-finder): add permissions, constraints, and usage examples"
```
```

---

### Task 8: 自审与最终验证

**Files:**
- Modify: `api-finder/SKILL.md` (如有修正)

- [ ] **Step 1: Spec 覆盖检查**

对照 spec 逐节核对：

| Spec 章节 | 对应 Task |
|-----------|----------|
| 一、整体架构与数据流 | Task 1 (目录结构+progress.json 结构) |
| 二、分析范围确定 | Task 2 |
| 三、任务恢复机制 | Task 2 (文件存在性检查规则+入口恢复) |
| 四、了解项目架构 | Task 3 |
| 五、接口继承 | Task 3 |
| 六、接口特征提取 | Task 4 |
| 七、架构识别接口 | Task 5 |
| 八、外部接口筛选 | Task 6 |
| 九、最终输出 | Task 6 |
| 十、附加要求 | Task 7 (权限+约束+示例) |

- [ ] **Step 2: 占位符扫描**

全文搜索以下模式，确认无遗漏：
```bash
grep -i "TODO\|TBD\|FIXME\|待定\|待补充" /home/admin/cc/wksp/siakam_security_skills/api_finder/api-finder/SKILL.md
```
预期：无匹配。

- [ ] **Step 3: 一致性检查**

检查以下关键点：
1. `progress.json` 中的 phase 值串起来是否形成完整流程：`arch_analysis → inherit → feature → arch_identify → filter → done`
2. 每个阶段的中间结果文件是否在后续阶段被正确引用（文件名一致）
3. 文件存在性检查规则是否在各阶段**每个关键文件检查点**都保持一致
4. `project_dir` 和 `<project_dir>` 占位符使用统一

- [ ] **Step 4: 如有修正，Commit**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/api_finder && git add api-finder/SKILL.md && git commit -m "fix(api-finder): address self-review findings"
```
