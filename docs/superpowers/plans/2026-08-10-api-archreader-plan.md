# api-archreader + api-finder 适配 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建独立 skill `api-archreader` 承担架构分析职责；修改 `api-finder` 删除自身架构分析阶段，改为消费 api-archreader 输出。

**Architecture:** 纯 markdown 文档变更——创建 `api-archreader/SKILL.md`，修改 `api-finder/SKILL.md`。api-archreader 输出 `arch.md` 到 `.ethunter_out/api-archreader/`；api-finder 通过前置依赖检查读取该文件，删除第三节全文，章节重编号。

**Tech Stack:** 纯 markdown 文本编辑

---

### 文件结构

| 文件 | 操作 | 职责 |
|------|------|------|
| `api-archreader/SKILL.md` | 新建 | 独立 skill，分析项目架构，输出 arch.md |
| `api-finder/SKILL.md` | 修改 | 删除第三节，新增前置依赖检查，全局路径替换，章节重编号 |

### api-archreader/SKILL.md 结构

```
核心原则 → 使用方法 → 输出 →
一、初始化与分析范围确定（4 个 Step，复用 api-finder 第一节）
二、了解项目架构（进入检查 → 浏览关键文件 → 三维分析 → 代码分区映射 → 输出 arch.md）
三、权限申请
四、约束规则
五、抗理性化检查
六、使用示例
```

与 api-finder 的核心差异：无 progress.json，无断点续分析，无任务恢复机制章节。架构分析本身（三维分析、arch.md 模板）保持一致。

---

### Task 1: 创建 api-archreader/SKILL.md — 前置部分

**Files:**
- Create: `api-archreader/SKILL.md`

- [ ] **Step 1: 创建目录并写入 YAML frontmatter + 核心原则 + 使用方法 + 输出**

```bash
mkdir -p api-archreader
```

写入文件内容：

```markdown
---
name: api-archreader
description: Use when the user invokes /api-archreader or asks to understand the architecture of an embedded/low-level C codebase (Linux kernel drivers, UEFI/BL31/BL2/XLoader firmware, ISP/SensorHub/GPU co-processor firmware). Analyzes module functionality, external communication boundaries, and code partition mapping, outputting a structured arch.md for downstream tools like api-finder.
---

# api-archreader

了解嵌入式/底层 C 项目的架构设计——分析模块功能定位、外部通信边界、代码分区映射，为 api-finder 等下游工具提供结构化的架构分析结果。

## 核心原则

1. **分析质量优先于分析效率。** 宁可慢，不可草率。每个关键文件必须完整深入阅读，不得因文件多而加速跳读。每个文件都要认真阅读和理解。

2. **架构分析的准确性决定后续所有分析的质量。** 外部通信边界的判断必须有代码证据支撑，不能仅凭文件名或目录名推测。

## 使用方法

```
/api-archreader <project_dir>
```

如果未指定 `project_dir`，默认为当前工作目录。

## 输出

分析完成后在 `<project_dir>/.ethunter_out/api-archreader/` 下生成：
- `arch.md` — 项目架构分析，包含模块概要、外部通信边界、代码分区映射
```

- [ ] **Step 2: 验证前置部分**

用 Read 读取文件确认内容写入正确。

- [ ] **Step 3: Commit**

```bash
git add api-archreader/SKILL.md
git commit -m "$(cat <<'EOF'
feat(api-archreader): add YAML frontmatter, principles, usage, and output sections

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: 创建 api-archreader/SKILL.md — 第一节（初始化）

**Files:**
- Modify: `api-archreader/SKILL.md`（追加内容）

- [ ] **Step 1: 追加第一节内容**

追加到文件末尾：

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

### Step 3: 检测 CodeGraph 可用性

用两种方法检查 `<project_dir>/.codegraph` 目录是否存在。如果存在，在后续所有代码分析中**优先使用 MCP 工具 `codegraph_explore`** 进行代码搜索和理解。如果不存在、或环境中未安装 codegraph 或未配置 MCP，使用常规工具（grep、find、Read、ast-grep），不中断分析。

### Step 4: 初始化输出目录

```bash
mkdir -p <project_dir>/.ethunter_out/api-archreader
```
```

- [ ] **Step 2: 验证追加内容**

用 Read 读取文件确认第一节内容完整。

- [ ] **Step 3: Commit**

```bash
git add api-archreader/SKILL.md
git commit -m "$(cat <<'EOF'
feat(api-archreader): add Section 1 — initialization and scope determination

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: 创建 api-archreader/SKILL.md — 第二节（架构分析）

**Files:**
- Modify: `api-archreader/SKILL.md`（追加内容）

- [ ] **Step 1: 追加第二节内容**

追加到文件末尾：

```markdown
---

## 二、了解项目架构

本阶段是 api-archreader 的核心任务。arch.md 的质量直接决定 api-finder 等下游工具的识别效果。

### 文件存在性检查规则

**禁止使用 `test -f` 或 `[ -f ]` 判断文件是否存在。** 每次需要判断关键文件是否存在时，必须使用至少两种不同方法交叉确认：

| 方法 | 示例 |
|------|------|
| `ls` 检查 | `ls <文件路径>` |
| `find` 检查 | `find <目录> -name "<文件名>" -maxdepth 1` |
| `Read` 工具 | 直接尝试 Read 读取文件 |

**判断规则：**

| 两次检查结果 | 处理方式 |
|-------------|---------|
| 都确认文件存在 | 文件存在，正常加载 |
| 都确认文件不存在（明确报错，如 "No such file or directory"、"NOT_FOUND"） | 文件不存在，正常跳过 |
| 一次存在、一次不存在 | 结果存在疑似，再使用第三种方法检查，取多数结果 |
| 两次都无输出或都报错 | **立即停止分析，向用户报告问题。不要假设文件缺失后继续分析！** |

### 进入检查

```
1. 用两种方法检查 <project_dir>/.ethunter_out/api-archreader/arch.md 是否存在。
   根据判断规则：
   ├── 确认存在 → 跳过本阶段，告知用户 arch.md 已存在。
   └── 确认不存在 → 执行架构分析
```

### 分析执行

1. **浏览关键文件：** 优先从 scope_files 中选取头文件（`.h`）开始阅读，了解模块对外暴露的类型定义和函数声明。然后选取几个核心的 `.c` 文件（优先选择 main 文件、init 文件、或文件名包含 `main`/`init`/`core`/`drv` 关键词的文件），了解模块的实现逻辑。

2. **分析三个维度：**

   - **模块概要：** 本模块的功能定位是什么？在系统中扮演什么角色？是内核驱动、UEFI固件、XLoader、还是协处理器固件（ISP/SensorHub/GPU）？主要职责有哪些？结合目录名、文件名、代码注释和函数命名惯例进行推断。

   - **外部通信边界：** 本模块与哪些外部实体有通信？通信方向是什么（外部→本模块、本模块→外部、双向）？通信信道是什么（系统调用/ioctl、共享内存、IPC消息队列、网络socket、硬件寄存器/MMIO、DMA缓冲区、GPIO/中断、文件系统）？结合代码中出现的通信相关 API（如 `copy_from_user`、`readl`/`writel`、`shm_open`、`msgsnd`/`msgrcv`、`recv`/`send`、`read`/`write` 等）进行推断。

   - **代码分区映射：** 按功能或通信方向将 scope_files 中的文件划分为不同的组。标注哪些文件最有可能包含外部接口（核心通信文件），哪些是内部实现（内部工具、配置管理、日志等，但也需要覆盖，因为可能包含间接的外部通信点）。

3. **生成任务文件列表：** 从代码分区映射中整理出一个完整的待分析文件列表（必须在 scope_files 范围内），按优先级排序：核心通信文件在前，内部实现文件在后。该列表供 api-finder 等下游工具使用。

4. **输出 arch.md：** 按照以下模板写入 `<project_dir>/.ethunter_out/api-archreader/arch.md`：

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
```

- [ ] **Step 2: 验证追加内容**

用 Read 读取文件确认第二节内容完整、arch.md 模板与 api-finder 第三节一致。

- [ ] **Step 3: Commit**

```bash
git add api-archreader/SKILL.md
git commit -m "$(cat <<'EOF'
feat(api-archreader): add Section 2 — architecture analysis with arch.md template

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: 创建 api-archreader/SKILL.md — 第三至六节（权限、约束、抗理性化、示例）

**Files:**
- Modify: `api-archreader/SKILL.md`（追加内容）

- [ ] **Step 1: 追加第三至六节内容**

追加到文件末尾：

```markdown
---

## 三、权限申请

本 skill 运行时需要以下工具权限：

| 工具 | 用途 |
|------|------|
| `Bash` | find 枚举文件、grep 搜索代码、ls 检查文件存在、mkdir 创建输出目录 |
| `Read` | 读取源代码文件、读取配置/状态文件 |
| `Write` | 写入 arch.md |
| `mcp__codegraph__codegraph_explore` | (可选) codegraph 可用时优先使用的代码探索工具 |
| `mcp__plugin_oh-my-claudecode_t__ast_grep_search` | (可选) ast-grep 代码模式搜索 |

如果环境中未安装 codegraph 或未配置 MCP，不中断分析，使用常规工具（grep、find、Read）代替。

---

## 四、约束规则

1. **所有分析由主 agent 完成，不使用 sub-agent 并行分析。** 不要调用 Agent 工具分派子任务。

2. **分析质量优先于分析效率。** 宁可慢不可草率。关键文件逐一认真阅读，大文件分批读取（每次 500-2000 行）。

3. **代码溯源要深入。** 判断通信边界、分析模块功能时，需要追踪代码中的通信 API 调用和数据结构定义，不要只看表面名称和注释就下结论。

4. **文件绝对路径。** arch.md 中的文件路径一律使用绝对路径。

5. **整个 skill 提示词用中文。** 相同语义的用词前后保持一致。

6. **适用业务：嵌入式/底层系统代码。** 适用于 Linux 内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU 固件。

7. **文件存在性检查严格遵循双方法+判断规则。** 禁止使用 `test -f` 或 `[ -f ]`，必须使用两种不同方法交叉确认。

---

## 五、抗理性化检查

当你在分析过程中产生以下想法时，STOP：

| 想法 | 现实 |
|------|------|
| "文件太多了，我加快一点" | 分析质量优先于效率。该读的代码一行不能少。 |
| "这个目录名看起来像是驱动，不用深入看了" | 必须深入阅读代码验证，不能凭文件名推测。 |
| "这个通信 API 出现了就是外部通信" | 需要确认 API 的数据流向和上下文，不能仅凭 API 名称判断。 |

---

## 六、使用示例

### 首次分析

```
用户: /api-archreader /srv/workspace/work_code/src

Agent:
  [初始化] 分析范围: 127 个 .c/.h 文件。codegraph 未检测到，使用常规工具。
  [进入检查] arch.md 不存在，开始架构分析。

  [了解项目架构]
    → 浏览关键头文件和核心实现文件...
    → 模块概要: ISP 固件，图像信号处理器，负责 RAW 数据预处理和 3A 算法
    → 外部通信边界: 识别出 3 个外部通信边界
      - SensorHub → ISP（共享内存，控制命令）
      - ISP → GPU 协处理器（DMA 缓冲区，图像数据）
      - 用户态 APP → ISP（ioctl，参数配置）
    → 代码分区映射: 48 个文件（核心通信 18 + 内部实现 30）

  分析完成。
  arch.md 已生成: /srv/workspace/work_code/src/.ethunter_out/api-archreader/arch.md
```

### 重复运行（arch.md 已存在）

```
用户: /api-archreader /srv/workspace/work_code/src

Agent:
  [初始化] 分析范围: 127 个 .c/.h 文件。
  [进入检查] arch.md 已存在，跳过分析。

  架构分析结果已存在，无需重新生成。
  如需重新分析，请手动删除 /srv/workspace/work_code/src/.ethunter_out/api-archreader/arch.md 后再次运行。
```
```

- [ ] **Step 2: 验证完整文件**

用 Read 读取完整文件，逐节确认内容正确：
- YAML frontmatter 格式
- 六个章节完整
- arch.md 模板与 api-finder 第三节一致
- 约束规则无冗余
- 抗理性化检查贴合场景

- [ ] **Step 3: Commit**

```bash
git add api-archreader/SKILL.md
git commit -m "$(cat <<'EOF'
feat(api-archreader): add Sections 3-6 — permissions, constraints, anti-rationalization, examples

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: 修改 api-finder/SKILL.md — 删除第三节

**Files:**
- Modify: `api-finder/SKILL.md`

- [ ] **Step 1: 删除第三节全文**

定位到 `api-finder/SKILL.md` 中从 `## 三、了解项目架构（arch_analysis）` 开始到 `---` 分隔线（`## 四、接口特征提取（feature）` 之前）的全部内容，删除。

删除范围包括：
- 第三节标题
- 进入检查
- 分析执行（步骤 1-5，含 arch.md 模板）
- 更新 progress.json 的步骤

保留第三节之前和之后的 `---` 分隔线。

- [ ] **Step 2: 验证删除**

用 Read 读取 api-finder/SKILL.md，确认：
- `## 三、了解项目架构（arch_analysis）` 已不存在
- `## 四、接口特征提取（feature）` 之前的内容已清除

- [ ] **Step 3: Commit**

```bash
git add api-finder/SKILL.md
git commit -m "$(cat <<'EOF'
refactor(api-finder): remove Section 3 arch_analysis, delegate to api-archreader

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: 修改 api-finder/SKILL.md — 新增前置依赖检查

**Files:**
- Modify: `api-finder/SKILL.md`

- [ ] **Step 1: 在恢复流程中插入 arch.md 依赖检查**

在第二节（任务恢复机制）的入口恢复流程中，找到步骤 1 和步骤 2 之间（即 progress.json 不存在/存在的分支判断之后，步骤 2 的"如果 phase 指向复杂阶段"之前）。不插入——直接在步骤 1 的"确认不存在 → 全新分析，从'三、了解项目架构'开始"修改为带检查的新描述。

具体修改：将入口恢复流程中的这段文字：

```
   1. 用两种方法检查 <project_dir>/.ethunter_out/api-finder/progress.json 是否存在。
      根据判断规则：
      ├── 确认不存在 → 全新分析，从"三、了解项目架构"开始
      └── 确认存在 → 读取 progress.json，找到当前 phase 字段的值

   2. 如果 phase 指向复杂阶段（feature / arch_identify / filter）：
```

替换为：

```
   1. 用两种方法检查 <project_dir>/.ethunter_out/api-finder/progress.json 是否存在。
      根据判断规则：
      ├── 确认不存在 → 全新分析
      └── 确认存在 → 读取 progress.json，找到当前 phase 字段的值

      向后兼容处理：如果 progress.json 存在且 phase = "arch_analysis"，
      将 phase 更新为 "feature"，移除 arch_analysis 状态字段，保存 progress.json。

   2. 前置依赖检查：用两种方法检查 <project_dir>/.ethunter_out/api-archreader/arch.md 是否存在。
      根据判断规则：
      ├── 确认存在 → 继续步骤 3
      └── 确认不存在 → **报错退出**。提示用户先运行 /api-archreader <project_dir>

   3. 如果 phase 指向复杂阶段（feature / arch_identify / filter）：
```

注意：步骤编号从 2 变成 3（原步骤 2 的内容不变，但编号 +1）。

- [ ] **Step 2: 同步更新步骤编号**

原入口恢复流程中的步骤 2 和步骤 3 编号需要分别更新为步骤 3 和步骤 4。检查并更新：

原 `2. 如果 phase 指向复杂阶段` → `3. 如果 phase 指向复杂阶段`
原 `3. 如果 phase = "done"` → `4. 如果 phase = "done"`

- [ ] **Step 3: 验证恢复流程**

用 Read 读取恢复流程部分，确认：
- 依赖检查在步骤 1 之后、步骤 3（原步骤 2）之前
- 步骤编号正确
- 向后兼容处理存在

- [ ] **Step 4: Commit**

```bash
git add api-finder/SKILL.md
git commit -m "$(cat <<'EOF'
feat(api-finder): add api-archreader/arch.md dependency check in recovery flow

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: 修改 api-finder/SKILL.md — 章节重编号 + 路径替换 + progress.json 更新

**Files:**
- Modify: `api-finder/SKILL.md`

- [ ] **Step 1: 章节重编号**

将以下章节标题逐一重编号：

| 原标题 | 新标题 |
|--------|--------|
| `## 四、接口特征提取（feature）` | `## 三、接口特征提取（feature）` |
| `## 五、架构识别接口（arch_identify）` | `## 四、架构识别接口（arch_identify）` |
| `## 六、外部接口筛选（filter）` | `## 五、外部接口筛选（filter）` |
| `## 七、权限申请` | `## 六、权限申请` |
| `## 八、约束规则` | `## 七、约束规则` |
| `## 九、使用示例` | `## 八、使用示例` |

使用 Edit 工具逐节替换，确保标题文本完全匹配。

- [ ] **Step 2: 替换所有 arch.md 引用路径**

搜索 `api-finder/SKILL.md` 中所有包含 `api-finder/arch.md` 的文本，替换为 `api-archreader/arch.md`：

- 第五节（arch_identify）的"全新启动 — 准备任务列表"步骤 1：`读取 arch.md` → 路径

  定位到 `## 四、架构识别接口（arch_identify）` 下的"读取 arch.md"描述，确认路径为 api-archreader 的。

  具体替换：`读取 arch.md 的"代码分区映射"部分` → 不变（arch.md 路径已在上下文确定），但需确认上下文中提到的 arch.md 路径正确。

  在整个文件中搜索 `<project_dir>/.ethunter_out/api-finder/arch.md`，全部替换为 `<project_dir>/.ethunter_out/api-archreader/arch.md`。

- [ ] **Step 3: 更新 progress.json 结构**

找到 progress.json 结构定义（在第二节中），将 phase 枚举从：

```json
"phase": "arch_analysis|feature|arch_identify|filter|done",
```

改为：

```json
"phase": "feature|arch_identify|filter|done",
```

同时移除 `arch_analysis` 状态字段：

```json
"arch_analysis": { "status": "pending|in_progress|completed" },
```

这行删除。

- [ ] **Step 4: 更新使用示例**

找到使用示例部分（第八节），在每个示例中加上 api-archreader 调用。

在"首次分析"示例的开头加入：

```
用户: /api-archreader /srv/workspace/work_code/src
→ api-archreader 完成，输出 arch.md 到 .ethunter_out/api-archreader/

用户: /api-fixer /srv/workspace/work_code/src
→ ...
```

并在阶段描述中去掉 `[阶段1/4] 了解项目架构...` 的相关输出行，将阶段数从 4 改为 3（原阶段 2→阶段 1，阶段 3→阶段 2，阶段 4→阶段 3）。

具体修改"首次分析"示例：

原：
```
  [恢复检查] 未发现 progress.json，全新分析开始。

  [阶段1/4] 了解项目架构...
    → 分析模块功能与通信边界...
    → arch.md 已生成（识别出 3 个外部通信边界，48 个待分析文件）

  [阶段2/4] 接口特征提取...
```

改为：

```
  [恢复检查] 未发现 progress.json，全新分析。
  [前置检查] api-archreader/arch.md 确认存在，继续。

  [阶段1/3] 接口特征提取...
```

同理修改"断点续分析"和"迭代分析"示例中的阶段数。

- [ ] **Step 5: 验证全部修改**

用 Read 读取 api-finder/SKILL.md 全文，确认：
- 章节编号连续（一到八）
- 所有 arch.md 路径指向 api-archreader
- progress.json 结构无 arch_analysis
- 使用示例阶段数正确
- 内容无遗留的原第三节引用

- [ ] **Step 6: Commit**

```bash
git add api-finder/SKILL.md
git commit -m "$(cat <<'EOF'
refactor(api-finder): renumber sections, replace arch.md paths, update progress.json

Remove arch_analysis from phase enum, point all arch.md references to
api-archreader output, update usage examples to show 3-phase flow.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: 最终验证 — 风格一致性检查

**Files:**
- 检查: `api-archreader/SKILL.md`
- 检查: `api-finder/SKILL.md`

- [ ] **Step 1: 对比检查三个 skill 的风格一致性**

用 Read 读取 `api-archreader/SKILL.md`、`api-cleaner/SKILL.md`、`api-fixer/SKILL.md`，重点对比：

| 检查项 | 说明 |
|--------|------|
| YAML frontmatter | name/description 字段格式一致 |
| 章节命名风格 | "一、xxx" 中文数字格式一致 |
| 代码块风格 | bash/json/markdown 代码块标记一致 |
| 权限申请表 | 表格列名和格式一致 |
| 约束规则编号 | 中文数字编号、规则描述风格一致 |
| 抗理性化检查 | "想法/现实" 表格格式一致 |
| 使用示例 | 终端输出格式、提示符（`用户:` / `Agent:`）一致 |
| 文件存在性检查规则 | 双方法表 + 判断规则表 格式一致 |
| 适用业务声明 | "嵌入式/底层系统代码" 描述一致 |

- [ ] **Step 2: 对比 api-archreader 的 arch.md 模板与 api-finder 原第三节**

确认模板完全一致（模块概要、外部通信边界表、代码分区映射 三个部分的标题和字段相同）。

- [ ] **Step 3: 修正不一致项**

如发现风格偏差，用 Edit 工具修正后 commit：

```bash
git add api-archreader/SKILL.md
git commit -m "$(cat <<'EOF'
style(api-archreader): align formatting with api-cleaner and api-fixer conventions

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 4: 最终 Commit（如无修改则跳过）**

```bash
git status
# 确认所有变更已提交，无遗漏
```
```

