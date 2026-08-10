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
