---
name: api-cleaner
description: Use when the user invokes /api-cleaner or asks to remove false positives, clean up, or verify external API interfaces identified by api-finder in embedded/low-level C codebases (Linux kernel drivers, UEFI/BL31/BL2/XLoader firmware, ISP/SensorHub/GPU co-processor firmware). Applies stricter exclusion rules to filter out internal implementation details misidentified as external interfaces.
---

# api-cleaner

消费 api-finder 的输出结果，对每个识别出的外部接口独立重新分析源码，应用更严格的排除规则，去除误报。适用业务场景：嵌入式/底层系统代码（Linux 内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU 固件）。

## 核心原则

1. **分析质量优先于分析效率。** 宁可慢，不可草率。每个接口的源码必须认真阅读和理解。

2. **更少的误报优先于更少的漏报。** 不确定时倾向于排除。只在有充分证据（代码模式 + 架构信息双重支撑）时才保留接口。

## 使用方法

```
/api-cleaner <project_dir>
```

如果未指定 `project_dir`，默认为当前工作目录。

## 输出

分析完成后在 `<project_dir>/.ethunter_out/api-cleaner/` 下生成：
- `api_clean.json` — 去误报后的外部接口列表
- `cleaner_summary.md` — 逐接口保留/排除理由说明
- `progress.json` — 断点续分析状态
- `tmp/analysis_state.json` — 逐接口分析中间状态

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

用两种方法交叉确认 api-finder 的输出文件是否存在。以下三个文件**必须全部存在**，任何一个缺失都应报错退出（无输入数据无法继续分析）：

- `<project_dir>/.ethunter_out/api-finder/api.json` — 外部接口列表（输入）
- `<project_dir>/.ethunter_out/api-finder/finder_summary.md` — 接口识别理由（参考）
- `<project_dir>/.ethunter_out/api-archreader/arch.md` — 项目架构分析（复用）

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

用两种方法检查 `<project_dir>/.codegraph` 目录是否存在。如果存在，在后续所有代码分析中**优先使用 MCP 工具 `codegraph_explore`** 进行代码搜索和理解（尤其是函数调用关系查询）。如果不存在、或环境中未安装 codegraph 或未配置 MCP，使用常规工具（grep、find、Read），不中断分析。

### Step 5: 初始化输出目录

```bash
mkdir -p <project_dir>/.ethunter_out/api-cleaner/tmp
```

### Step 6: 加载输入并判断任务恢复

1. 读取 `<project_dir>/.ethunter_out/api-finder/api.json`，获取待分析接口列表。格式为：

```json
[
  {"name": "<函数名>", "file": "<文件绝对路径>"},
  ...
]
```

2. 用两种方法检查 `<project_dir>/.ethunter_out/api-cleaner/progress.json` 是否存在：

```
├── 确认不存在 → 全新分析。将 api.json 中所有接口写入 analysis_state.json，全部 status = "pending"
└── 确认存在 → 加载 progress.json 和 analysis_state.json，找到 status = "in_progress" 或 "pending" 的条目继续分析
```

### progress.json 结构

```json
{
  "phase": "analyzing|done",
  "api_total": 15,
  "api_analyzed": 5,
  "api_kept": 3
}
```

### analysis_state.json 结构

```json
{
  "api_list": [
    {
      "index": 1,
      "name": "func_a",
      "file": "/abs/path/to/file.c",
      "status": "completed|in_progress|pending",
      "form": null,
      "decision": null,
      "reason": null,
      "taint_data": null
    }
  ]
}
```

全新分析时初始化：
- `api_total` = api.json 条目数
- `api_analyzed` = 0
- `api_kept` = 0
- `phase` = `"analyzing"`
- `api_list` 中每个条目：index 递增（1-based），status = `"pending"`，form/decision/reason 均为 null

3. 更新 progress.json 中 `phase = "analyzing"`，保存。

---

## 二、逐接口分析（核心）

对 `analysis_state.json` 中每个 `status = "pending"` 的接口，按 index 顺序逐个执行以下分析。每分析完一个接口**立即保存** `analysis_state.json` 和 `progress.json`，防止中断丢失进度。

开始分析一个接口时，将其 status 更新为 `"in_progress"` 并保存。

### 2.1 读取源码

定位到函数定义处，读取完整函数体代码。

- 优先使用 codegraph_explore 查询函数定义位置
- codegraph 不可用时，使用 `grep -rn "<函数名>" <文件路径>` 定位函数定义行，然后用 Read 读取该函数完整代码
- 如果文件超过 2000 行，分批读取（每次 2000 行）

### 2.1.1 文件校验

读取源码后，执行以下两层校验，任何一层不通过则直接排除该接口，跳到下一个接口处理：

1. **函数定义验证：** 确认 `file` 指向的文件中包含该函数定义（函数名后跟 `(` 且最终包含 `{` 的函数体定义）。
   - 包含函数定义 → 通过，`file` 不变
   - 不包含函数定义 → 在全部 scope_files 中搜索该函数定义
     - 找到 → 更新 `file` 为正确的定义文件绝对路径，同步更新 analysis_state.json 中的 `file` 字段
     - 找不到 → `decision` 改为 `"exclude"`，`reason` 记录 `"函数定义未找到"`，跳过后续分析

2. **分析范围验证：** 校验 `file` 是否在 scope_files 列表中。
   - 在 scope_files 中 → 通过，进入 2.2 快速排除检查
   - 不在 scope_files 中 → `decision` 改为 `"exclude"`，`reason` 记录 `"接口不在代码分析范围内"`

如果校验中修改了 `file` 字段，立即保存 analysis_state.json。

### 2.2 快速排除检查

先执行轻量级排除，命中任一规则则直接记录 decision = `"exclude"`，跳过后续深入分析：

**规则一：测试函数排除**

函数名包含以下前缀或后缀：`test_`、`_test`、`mock_`、`stub_`、`demo_`、`sample_`；
或函数所在文件位于 `test/`、`tests/`、`unittest/`、`mock/` 目录下；
或代码注释标记为"测试用"、"for test only"、"调试用"；
或函数仅在测试/调试代码中被引用。

LLM 可根据上下文灵活判断，即使不完全匹配以上规则。

**规则二：冗余函数排除**

函数体为空（只包含 `return;` 或不含任何有效代码）；
或函数仅调用另一个完全相同签名的函数（纯转发/包装）；
或函数被注释标记为 `deprecated`、`unused`、`不再使用`；
或函数仅包含日志打印或调试输出语句，无实际数据处理逻辑。

LLM 可根据代码语义灵活判断。

**规则三：无任何入参且函数体无信道读取**

搜索函数体代码，未发现任何信道读取相关的 API 调用（信道 API 列表见 2.3 节）。如果既无入参也无信道读取 → **排除**。

### 2.3 判断接口形式（form）

阅读函数代码，判断该接口属于哪种外部输入形式。一个接口可以同时属于两种形式（form = `"both"`）。

**parameter_input 判断：**

函数是否有入参？如果有，参数中是否存在可能携带外部数据的指针/缓冲区/结构体？需注意：
- 系统框架类参数（如 `struct file *f`、`struct inode *i`）不是外部输入
- 输出类参数（如 `void *out_buffer`、`int *result`）不是外部输入
- api-finder 给出的 form 类型（如果 finder_summary.md 中有记录）仅供参考和验证，cleaner 独立判断

**channel_read 判断：**

函数体内是否调用了从外部信道读取数据的操作？注意：仅关心**读取**外部数据的操作，向外部**写入**数据的操作不算外部输入。

搜索以下信道读取模式：
- 共享内存读取：`shm_open`、`shmget`、`shmat`、`mmap` + 后续读取操作
- 寄存器/MMIO读取：`readl`、`readw`、`readb`、`ioread32`、`MMIO_READ` 等
- IPC消息接收：`msgrcv`、`mq_receive`、`recv`、`recvfrom`、`recvmsg`
- 文件读取：`read`、`fread`、`pread`、`aio_read`
- DMA缓冲区读取：`dma_alloc_coherent` + 后续读取、mmap dma buffer
- 网络socket读取：`recv`、`recvfrom`、`read` from socket fd
- 管道/FIFO读取：`read` from pipe fd

**两者都不是 → 直接排除。** 记录 form = `"none"`，decision = `"exclude"`，reason 写明无外部输入来源的证据。

### 2.4 parameter_input 深入分析

仅当 form 包含 `parameter_input` 时执行。

#### a. 内部调用者检查（关键新增规则）

在 `project_dir` 范围内搜索该函数的所有引用点：
- 优先用 `codegraph_explore` 查询函数引用/调用关系
- codegraph 不可用时用 `grep -rn "<函数名>" <project_dir>` 在 project_dir 中搜索

对找到的每个引用点，读取其上下文代码（至少前后 5 行），判断引用形式：

| 引用形式 | 判断 | 处理 |
|---------|------|------|
| 普通函数调用 `func(args)` | **内部有调用者** | → **直接排除该接口** |
| 注册点：赋值给结构体字段 `.field = func` 或数组元素 `{CMD, func}` | 不是内部调用 | 继续 |
| 注册点：作为参数传给注册函数 `register(func)` 或 `register(&func)` | 不是内部调用 | 继续 |
| 注册点：取地址 `&func` 并赋值给某变量/字段 | 不是内部调用 | 继续 |
| 函数声明、头文件声明 | 不是调用 | 跳过 |
| 注释中提及、文档引用 | 不是调用 | 跳过 |

**核心逻辑：** 如果函数 f1 在 project_dir 中调用了该接口 f2，则 f2 的参数来自 f1 的传递，f1 比 f2 更适合作为外部接口。该接口属于内部实现细节，应排除。

**注意区分"注册"和"调用"：**
- `register_handler(my_handler)` → my_handler 是作为参数传递给注册函数，不是被调用，`my_handler` 仍可能是外部接口
- `my_handler(data)` → my_handler 被直接调用，调用者传入数据，`my_handler` 的内部性得到确认，应排除

#### b. 锁定外部输入参数列表

对函数签名中的每个入参逐一分析，筛选出外部输入参数：

对每个参数回答以下两个问题：
1. 该参数是系统框架类参数吗？（`struct file *f`、`struct inode *i`、内核内部数据结构指针、框架回调约定的上下文参数等）→ 是 → **内部参数，跳过**
2. 该参数用于输出目的吗？（`void *out_buffer`、`int *result`、`void *ret_data`），代码中对该参数仅写入不读取 → 是 → **内部参数，跳过**

两个都否 → 该参数为**外部输入参数**，加入外部输入参数列表。

统计外部输入参数列表：
- 个数 = 0 → **直接排除**（无外部输入），记录 reason
- 个数 > 0 → 记录参数名到 `taint_data` 字段（多个以 `/` 分割），进入 c. 外部输入参数个数与类型裁决

`taint_data` 示例：`"data/len"`（两个参数都是外部输入）、`"buf"`（仅 buf 是外部输入）。

#### c. 外部输入参数个数与类型裁决

先统计 b 产出的外部输入参数列表个数，再决定是否做类型判断。**类型判断只看参数本身的 C 声明类型，绝不追踪函数体内从 struct/union 取出的成员类型：**

- `struct req *r` → 参数本身是指针，不是整数型，即使函数体内使用了 `r->cmd_id`
- `void *data` → 参数本身是指针，不是整数型
- `int fd` → 参数本身是整数型

1. **个数 > 1**：多个外部输入参数，安全影响显著 → **无需额外判断，保留接口**

2. **个数 = 1**：唯一的外部输入参数，需判断其类型：
   - **非整数型**（指针、结构体指针、缓冲区、数组等）→ **保留接口**。指针类型本身就是外部数据的入口载体，无需进一步验证
   - **整数型**（含枚举、布尔）→ 需要判断该整数是否为内存地址：
     - 是内存地址 → **保留**
     - 非内存地址（仅作计数器、标志位、长度、命令码等）→ **排除**

3. **内存地址判断规则**：
   - 视为**内存地址，保留**：参数类型为 `uintptr_t`、`unsigned long`、`size_t` 等，且在函数体内被显式强转为指针类型（如 `(void *)arg`、`(struct foo *)arg`）；或被用于解引用（`*ptr`）、传给 `memcpy`/`memset`/`copy_from_user`/`mmap` 等内存操作函数的地址参数位置
   - 视为**普通整数值，排除**（安全影响小）：仅作为计数器、标志位（flags）、长度（len/size）；仅用于算术运算或比较操作；是枚举值、命令码（cmd/opcode）等

   **判断要仔细：** 即使参数名是 `addr`/`address`/`ptr`/`base`，也要看代码中实际如何使用。类型为 `unsigned long` 时尤其要注意——在 64 位系统中 `unsigned long` 常被用作指针存储类型。

#### d. 通信边界关联验证

参考 `<project_dir>/.ethunter_out/api-finder/finder_summary.md` 中该接口的识别理由，以及 `<project_dir>/.ethunter_out/api-archreader/arch.md` 中的"外部通信边界"表。

确认该接口是否能与 arch.md 中的某个通信边界明确关联：
- 该接口与哪个外部模块/实体通信？
- 函数的参数/数据是否与该通信信道的数据格式匹配？

**无法与任何通信边界明确关联 → 排除。**

### 2.5 channel_read 深入分析

仅当 form 包含 `channel_read` 时执行。

#### a. 确认读方向

区分信道的读和写操作：
- **读取操作 → 外部输入**：`msgrcv`、`recv`、`read`、`readl`、`ioread32`、`fread` 等
- **写入操作 → 不算外部输入**：`msgsnd`、`send`、`write`、`writel`、`iowrite32`、`fwrite` 等

如果函数体内只有写入操作没有读取操作 → 该接口无外部输入（form 判断可能有误），修正判断。

如果函数体内同时有读取和写入操作 → **保留**（只要存在读取操作，就有外部输入）。

将信道读取的变量/来源记录到 `taint_data` 字段。例如：`"共享内存 shm_ptr"`、`"IPC msgrcv → msg_buf"`、`"readl(MMIO_BASE) → status_reg"`。

#### b. 单整数读取排除

如果从信道**仅读取了一个整数数据**（且非内存地址），→ **排除**。

判断方式参考 2.4.c（整数型参数特殊处理）。从信道读取的大部分情况下是一整段 buffer 数据，如果仅读取单个整数（如状态寄存器值），安全影响较小。

### 2.6 综合裁决

综合以上所有分析，给出最终结论：

| 结论 | 条件 |
|------|------|
| **保留（keep）** | 未命中任何排除规则，有明确外部输入（参数或信道），与通信边界明确关联 |
| **排除（exclude）** | 命中任一排除规则 |

**规则判断边界模糊时 → 倾向排除**（误报优先原则）。

### 2.7 记录结论

更新 `analysis_state.json` 中当前接口的条目：

```json
{
  "index": 1,
  "name": "func_a",
  "file": "/abs/path/to/file.c",
  "status": "completed",
  "form": "parameter_input|channel_read|both|none",
  "decision": "keep|exclude",
  "reason": "<具体证据，不使用模糊描述>",
  "taint_data": "<外部输入变量名或来源>"
}
```

**reason 字段要求：**
- ✅ 好的 reason：`"data参数是用户态通过ioctl传入的缓冲区指针，来自copy_from_user填充后的内存，与用户态APP→内核驱动的通信边界关联，无内部调用者"`
- ✅ 好的 reason：`"被内部函数 internal_dispatcher 直接调用（src/dispatch.c:156），参数来自内部传递，internal_dispatcher 更适合作为外部接口"`
- ❌ 不好的 reason：`"看起来像外部接口"`、`"可能接收外部数据"`、`"应该是一个handler"`

同时更新 `progress.json`：
- `api_analyzed` += 1
- 如果 decision = `"keep"`：`api_kept` += 1

**每分析完一个接口立即保存两个 JSON 文件。** 然后继续下一个 status = `"pending"` 的接口，直到全部接口分析完毕。

---

## 三、防止任务遗忘

本 skill 涉及长任务（逐个分析多个接口），通过以下机制防止任务遗忘：

1. **中间状态持久化：** 每分析完一个接口立即将 `analysis_state.json` 和 `progress.json` 写入磁盘。即使分析中断，已完成的接口不会丢失进度。

2. **status 追踪：** 每个接口有明确的状态标记（pending → in_progress → completed），恢复时只需找到第一个 `status = "in_progress"` 或 `status = "pending"` 的接口继续分析。

3. **关键数字追踪：** progress.json 中的 `api_total`、`api_analyzed`、`api_kept` 提供了整体进度概览。恢复时交叉验证：如果 `api_analyzed` 与 `analysis_state.json` 中 `status = "completed"` 的条目数不一致，以 `analysis_state.json` 的实际状态为准，修正 progress.json。

4. **恢复入口：** 在分析每个接口之前，先根据 progress.json 判断是全新分析还是断点恢复。恢复时重新读取 analysis_state.json，从断点继续。

---

## 四、最终输出

**在生成任何输出文件之前，必须重新阅读本章节的输出字段定义。不得凭记忆直接输出。**

全部接口分析完成后（`api_list` 中所有条目 status = `"completed"`），生成两个输出文件。

### 4.1 api_clean.json

从 `analysis_state.json` 中 `api_list` 提取所有 `decision = "keep"` 的条目，输出到 `<project_dir>/.ethunter_out/api-cleaner/api_clean.json`。

每个条目包含以下 **4 个字段**，直接从 `analysis_state.json` 对应条目取值：

| 字段 | 来源 | 说明 |
|------|------|------|
| `name` | `analysis_state.json` 的 `name` | 函数名 |
| `file` | `analysis_state.json` 的 `file` | 文件绝对路径 |
| `form` | `analysis_state.json` 的 `form` | `parameter_input` / `channel_read` / `both` |
| `taint_data` | `analysis_state.json` 的 `taint_data` | 外部输入变量名或来源 |

其余字段（`index`、`status`、`decision`、`reason`）不输出。

```json
[
  {"name": "func_a", "file": "/abs/path/to/a.c", "form": "parameter_input", "taint_data": "data/len"},
  {"name": "func_c", "file": "/abs/path/to/c.c", "form": "channel_read", "taint_data": "共享内存 shm_ptr"}
]
```

### 4.2 cleaner_summary.md

写入 `<project_dir>/.ethunter_out/api-cleaner/cleaner_summary.md`：

```markdown
# 接口去误报分析报告 — <project_dir>

## 分析概要
- 输入接口数: <api_total>
- 保留: <api_kept>
- 排除: <api_total - api_kept>

## 保留接口

### 1. <函数名> — <文件绝对路径>
- **接口形式:** parameter_input|channel_read|both
- **通信边界:** <该接口与哪个外部模块通信，通过什么信道>
- **外部输入:** <接收什么外部输入，来自哪里>
- **保留原因:** <综合裁决理由>

（保留接口按 api.json 中的原始顺序排列）

## 排除接口

### <编号>. <函数名> — <文件绝对路径>
- **排除原因:** <具体排除证据>
```

### 4.3 完成

更新 `progress.json`：`phase = "done"`。

告知用户分析完成，报告关键数字：
- 输入接口数、保留数、排除数
- 输出文件路径：`<project_dir>/.ethunter_out/api-cleaner/api_clean.json` 和 `cleaner_summary.md`

---

## 五、约束规则

1. **所有分析由主 agent 完成，不使用 sub-agent 并行分析。** 不要调用 Agent 工具分派子任务。

2. **分析质量优先于分析效率。** 宁可慢不可草率。每个接口的源码必须认真阅读，大文件分批读取（每次 500-2000 行）。

3. **误报优先原则。** 规则判断边界模糊时倾向排除。不确定就排除。

4. **reason 字段必须具体。** 不使用"看起来像"、"可能是"、"应该"等模糊描述。每条原因必须包含具体的代码证据。

5. **逐接口保存状态。** 每分析完一个接口立即写入 analysis_state.json 和 progress.json，防止中断丢失进度。

6. **文件存在性检查严格遵循双方法+判断规则。** 禁止使用 `test -f` 或 `[ -f ]`，必须使用两种不同方法交叉确认。

7. **文件绝对路径。** 所有 JSON 中的 file 字段一律使用绝对路径。

8. **SKILL 全部提示词使用中文。** 相同语义的用词前后保持一致。

9. **代码溯源要深入。** 判断调用关系、分析参数用途时，需要追踪代码定义和使用点，不要只看表面名称和注释就下结论。

10. **适用业务。** 本 skill 适用于嵌入式/底层系统代码（Linux 内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU 固件）。

---

## 六、权限申请

本 skill 运行时需要以下工具权限：

| 工具 | 用途 |
|------|------|
| `Bash` | find 枚举文件、grep 搜索代码/调用关系、ls 检查文件、mkdir 创建目录 |
| `Read` | 读取源码、api.json、finder_summary.md、arch.md、各阶段 JSON 状态文件。arch.md 位于 `.ethunter_out/api-archreader/` 下 |
| `Write` | 写入 progress.json、analysis_state.json、api_clean.json、cleaner_summary.md |
| `mcp__codegraph__codegraph_explore` | (可选) codegraph 可用时优先使用的代码探索工具 |
| `mcp__plugin_oh-my-claudecode_t__ast_grep_search` | (可选) ast-grep 代码模式搜索 |

如果环境中未安装 codegraph 或未配置 MCP，不中断分析，使用常规工具（grep、find、Read）代替。

---

## 七、抗理性化检查

当你在分析过程中产生以下想法时，STOP：

| 想法 | 现实 |
|------|------|
| "接口太多了，我加快一点" | 分析质量优先于效率。每个接口的源码一行不能少。 |
| "这个接口看起来就是外部接口，不用深入查了" | 必须深入阅读验证，不能凭感觉判断。 |
| "api-finder 的 finder_summary.md 说这是外部接口，直接保留吧" | cleaner 需要独立分析，finder_summary.md 仅作参考。 |
| "这个函数可能也是测试函数，但不太确定，先保留吧" | 不确定时倾向排除。误报优先原则。 |
| "内部调用者判断可能不太准确，先保留吧" | 引用点必须逐一排查，确认无内部调用者才能保留。 |
| "这个状态文件检查结果不太确定，先继续吧" | 文件存在性检查必须严格遵循双方法+判断规则。 |

---

## 八、使用示例

### 首次分析

```
用户: /api-cleaner /srv/workspace/work_code/src

Agent:
  [初始化] 分析范围: 127 个 .c/.h 文件。
  [输入检查] api.json (15个接口)、finder_summary.md、arch.md 均存在。
  [恢复检查] 未发现 progress.json，全新分析开始。

  [逐接口分析] 15 个待分析接口...
    → 接口 1/15: func_a — 保留 (parameter_input, 与ioctl边界关联)
    → 接口 2/15: func_b — 排除 (测试函数, test/ 目录下)
    → 接口 3/15: func_c — 排除 (被 func_x 内部调用, src/dispatch.c:156)
    → 接口 4/15: func_d — 排除 (仅有 int flags 一个外部输入参数, 非内存地址)
    → 接口 5/15: func_e — 排除 (无入参且无信道读取)
    → ...
    → 接口 15/15: func_o — 保留 (channel_read, 从共享内存读取)

  分析完成。
  输入: 15 个接口 | 保留: 6 | 排除: 9
  输出: /srv/workspace/work_code/src/.ethunter_out/api-cleaner/api_clean.json
        /srv/workspace/work_code/src/.ethunter_out/api-cleaner/cleaner_summary.md
```

### 断点续分析

```
用户: /api-cleaner /srv/workspace/work_code/src

Agent:
  [初始化] 分析范围: 127 个 .c/.h 文件。
  [输入检查] api.json、finder_summary.md、arch.md 均存在。
  [恢复检查] 发现 progress.json，api_analyzed = 7/15，从接口 8 继续。

  [逐接口分析] 剩余 8 个接口...
    → 接口 8/15: func_h — 保留
    → ...
    → 接口 15/15: func_o — 保留

  分析完成。保留: 6 | 排除: 9
```
