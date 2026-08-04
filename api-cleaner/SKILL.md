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
- `api-clean.json` — 去误报后的外部接口列表
- `summary.md` — 逐接口保留/排除理由说明
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
- `<project_dir>/.ethunter_out/api-finder/summary.md` — 接口识别理由（参考）
- `<project_dir>/.ethunter_out/api-finder/arch.md` — 项目架构分析（复用）

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
      "reason": null
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
- api-finder 给出的 form 类型（如果 summary.md 中有记录）仅供参考和验证，cleaner 独立判断

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

在 `scope_files` 范围内搜索该函数的所有引用点：
- 优先用 `codegraph_explore` 查询函数引用/调用关系
- codegraph 不可用时用 `grep -rn "<函数名>" <project_dir>` 在 scope_files 中搜索

对找到的每个引用点，读取其上下文代码（至少前后 5 行），判断引用形式：

| 引用形式 | 判断 | 处理 |
|---------|------|------|
| 普通函数调用 `func(args)` | **内部有调用者** | → **直接排除该接口** |
| 注册点：赋值给结构体字段 `.field = func` 或数组元素 `{CMD, func}` | 不是内部调用 | 继续 |
| 注册点：作为参数传给注册函数 `register(func)` 或 `register(&func)` | 不是内部调用 | 继续 |
| 注册点：取地址 `&func` 并赋值给某变量/字段 | 不是内部调用 | 继续 |
| 函数声明、头文件声明 | 不是调用 | 跳过 |
| 注释中提及、文档引用 | 不是调用 | 跳过 |

**核心逻辑：** 如果函数 f1 在 scope_files 中调用了该接口 f2，则 f2 的参数来自 f1 的传递，f1 比 f2 更适合作为外部接口。该接口属于内部实现细节，应排除。

**注意区分"注册"和"调用"：**
- `register_handler(my_handler)` → my_handler 是作为参数传递给注册函数，不是被调用，`my_handler` 仍可能是外部接口
- `my_handler(data)` → my_handler 被直接调用，调用者传入数据，`my_handler` 的内部性得到确认，应排除

#### b. 逐参数外部输入判断

对函数签名中的每个入参逐一分析，判断其是否为外部输入：

对每个参数回答以下问题：
1. 该参数是系统框架类参数吗？（`struct file *f`、`struct inode *i`、内核内部数据结构指针、框架回调约定的上下文参数等）→ 是 → **内部参数，跳过**
2. 该参数用于输出目的吗？（`void *out_buffer`、`int *result`、`void *ret_data`），代码中对该参数仅写入不读取 → 是 → **内部参数，跳过**
3. 该参数是指针/缓冲区/结构体指针吗？（`void *data`、`char *buf`、自定义结构体指针），代码中有对该指针指向内容的读取操作 → 是 → **外部输入参数**
4. 该参数是整数型吗？→ 是 → 进入 c. 判断

记录外部输入参数列表。如果列表为空 → **排除**（无外部输入）。

**重要：** 大多数情况下，外部数据以指针形式传递。判断时要仔细分析每个参数的实际用途，不要看到有参数就认为有外部输入。

#### c. 整数型参数特殊处理

如果所有外部输入参数**仅有一个整数型参数**（其他参数都是内部参数），需要判断该整数是否为内存地址：

以下情况视为**内存地址，保留**：
- 参数类型为 `uintptr_t`、`unsigned long`、`size_t` 等，且在函数体内被显式强转为指针类型（如 `(void *)arg`、`(struct foo *)arg`）
- 参数在函数体内被用于内存访问操作：解引用（`*ptr`）、传给 `memcpy`/`memset`/`copy_from_user`/`mmap` 等内存操作函数的地址参数位置

以下情况视为**普通整数值，排除**（安全影响小）：
- 参数仅作为计数器、标志位（flags）、长度（len/size）使用
- 参数仅用于算术运算或比较操作
- 参数是枚举值、命令码（cmd/opcode）等

**判断要仔细：** 即使参数名是 `addr`/`address`/`ptr`/`base`，也要看代码中实际如何使用。类型为 `unsigned long` 时尤其要注意——在 64 位系统中 `unsigned long` 常被用作指针存储类型。

#### d. 通信边界关联验证

参考 `<project_dir>/.ethunter_out/api-finder/summary.md` 中该接口的识别理由，以及 `<project_dir>/.ethunter_out/api-finder/arch.md` 中的"外部通信边界"表。

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
  "reason": "<具体证据，不使用模糊描述>"
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
