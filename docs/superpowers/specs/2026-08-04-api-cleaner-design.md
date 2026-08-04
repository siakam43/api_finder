# api-cleaner Skill 设计文档

## 概述

api-cleaner 是一个纯 markdown 的 Claude Code skill（`SKILL.md`），位于 `api-cleaner/` 目录下。它消费 api-finder 的输出结果，对每个识别出的外部接口独立重新分析源码，应用更严格的排除规则，去除误报。

适用业务场景：嵌入式/底层系统代码（Linux 内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU 固件）。

**核心原则：** 更少的误报优先于更少的漏报。不确定时倾向于排除。

---

## 一、整体架构与数据流

### 目录结构

```
api-cleaner/
└── SKILL.md              # skill 全部内容

# 运行时在待分析项目中生成：
project_dir/.ethunter_out/api-cleaner/
├── api-clean.json         # 最终输出：去误报后的外部接口列表
├── summary.md             # 最终输出：逐接口保留/排除理由
├── progress.json          # 断点续分析状态
└── tmp/
    └── analysis_state.json  # 逐接口分析进度（中间状态）
```

### 数据流

```
api-finder 输出                        api-cleaner 输出
─────────────                        ─────────────────
api.json ─────────┐                  
summary.md ───────┼──→ [api-cleaner 逐接口分析] ──→ api-clean.json
arch.md ──────────┘                               summary.md
```

### 与 api-finder 的关系

| 数据 | 来源 | 使用方式 |
|------|------|---------|
| `arch.md` | api-finder 输出 | **直接复用**，不重新分析架构 |
| `api.json` | api-finder 输出 | 作为待分析的接口列表 |
| `summary.md` | api-finder 输出 | 参考其中的通信边界信息，仅做验证 |
| 源码文件 | scope_files | **独立读取分析**（参数、调用关系、测试/冗余判断） |

---

## 二、初始化流程

### Step 1: 解析输入参数

```
/api-cleaner <project_dir>
```

`project_dir` 未指定时默认为当前工作目录。

### Step 2: 确定代码分析范围

与 api-finder 完全一致的逻辑：

```
检查 project_dir/.ethunter_out/
├── clean_code.txt 存在 → 读取文件路径列表，去重，处理相对/绝对路径
├── clean_code.txt 不存在但 .etignore 存在 → find *.c *.h，按 .etignore 排除
└── 两者都不存在 → find *.c *.h 全部作为分析范围
```

### Step 3: 检查输入文件

用两种方法交叉确认 `project_dir/.ethunter_out/api-finder/` 下的三个文件：

- `api.json` — 必须存在，否则报错退出（没有输入数据）
- `summary.md` — 必须存在，否则报错退出
- `arch.md` — 必须存在，否则报错退出（架构信息是分析基石）

### Step 4: 检测 CodeGraph 可用性

检查 `project_dir/.codegraph` 是否存在。存在则优先使用 `codegraph_explore`，不存在或未配置 MCP 则使用常规工具。

### Step 5: 初始化输出目录

```bash
mkdir -p <project_dir>/.ethunter_out/api-cleaner/tmp
```

### Step 6: 任务恢复检查

```
用两种方法检查 progress.json 是否存在：
├── 确认不存在 → 全新分析，生成待分析接口列表，初始化 analysis_state.json
└── 确认存在 → 加载 progress.json 和 analysis_state.json，从中断位置恢复
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

初始化时从 `api.json` 读取全部接口写入 `api_list`，status 均为 `pending`。

### 文件存在性检查规则（复用 api-finder 规则）

禁止使用 `test -f` 或 `[ -f ]` 判断文件是否存在。必须使用至少两种不同方法交叉确认（`ls` / `find` / `Read` 工具）。两次检查结果不一致时使用第三种方法，取多数结果。两次都无输出或都报错则停止分析，报告用户。

---

## 三、逐接口分析流程（核心）

对 `analysis_state.json` 中每个 `status = "pending"` 的接口，按 index 顺序逐个执行以下分析。每分析完一个接口立即保存 `analysis_state.json`。

### 3.1 读取源码

定位到函数定义处，读取完整函数体代码。

### 3.2 快速排除检查

先执行轻量级排除，命中则直接跳过后续深入分析：

| 规则 | 判断方式 | 命中结论 |
|------|---------|---------|
| 测试函数 | 函数名含 `test_`/`_test`/`mock_`/`stub_`/`demo_`/`sample_`，或文件位于 `test/`/`tests/`/`unittest/`/`mock/` 目录，或注释标记为"测试用"/"for test only"/"调试用" | **排除** |
| 冗余函数 | 函数体为空、仅调用同签名函数（纯转发）、注释标记 `deprecated`/`unused`/`不再使用`、仅含日志/调试打印无实际逻辑 | **排除** |
| 无任何入参且函数体无信道读取 | 搜索函数体，未发现任何信道读取 API 调用 | **排除** |

### 3.3 判断接口形式（form）

阅读函数代码，判断该接口属于哪种外部输入形式（可同时属于两种）：

**parameter_input 判断：** 函数是否有入参？如果有，参数中是否存在可能携带外部数据的指针/缓冲区/结构体？

**channel_read 判断：** 函数体内是否调用了从外部信道读取数据的操作？搜索以下模式：
- 共享内存：`shm_open`/`shmget`/`shmat`/`mmap` + 读取
- MMIO/寄存器：`readl`/`readw`/`readb`/`ioread32`/`MMIO_READ` 等
- IPC：`msgrcv`/`mq_receive`/`recv`/`recvfrom`/`recvmsg`
- 文件：`read`/`fread`/`pread`/`aio_read`
- DMA：`dma_alloc_coherent` + 读取 / `mmap` dma buffer
- 网络：`recv`/`recvfrom` / `read` from socket fd

**两者都不是 → 直接排除**（无外部输入）。

### 3.4 深入分析

#### 3.4.1 parameter_input 深入分析

**a. 内部调用者检查（关键新增规则）**

在 `scope_files` 范围内搜索该函数的所有引用点：
- 优先用 `codegraph_explore` 查询调用关系
- 否则用 `grep -rn "函数名"` 搜索

对每个引用点判断：

| 引用形式 | 判断 |
|---------|------|
| 普通函数调用 `func(args)` | **存在内部调用者 → 直接排除该接口** |
| 注册点（赋值给结构体字段/数组元素、作为参数传给注册函数、取地址赋值） | 不是内部调用，继续 |
| 函数声明/头文件声明/注释 | 跳过 |

核心逻辑：f1 调用了 f2，则 f2 的参数来自 f1 的传递，f1 比 f2 更适合作为外部接口。

**b. 逐参数外部输入判断**

对每个入参判断是否携带外部输入：

| 参数类型 | 判断 |
|---------|------|
| 系统框架类参数（`struct file *f`、`struct inode *i`、内核内部数据结构指针、框架回调上下文参数） | **内部参数，跳过** |
| 输出类参数（`void *out_buffer`、`int *result`、`void *ret_data`），代码中仅写入不读取 | **内部参数，跳过** |
| 指针/缓冲区/结构体指针（`void *data`、`char *buf`、自定义结构体指针），代码中有读取操作 | **外部输入** |
| 整数型参数 | 见下方 c 判断 |

**c. 整数型参数特殊处理**

如果所有外部输入参数中仅有一个整数型参数（即使还有其他内部参数），做内存地址判断：

- 参数类型为 `uintptr_t`/`unsigned long`/`size_t`，且在代码中被强转为指针 → **内存地址，视为外部输入，保留**
- 参数被用于内存访问操作（解引用、传给 `memcpy`/`memset`/`copy_from_user` 等） → **内存地址，视为外部输入，保留**
- 普通整数值（计数器、标志位、长度等） → **排除**（安全影响小）

**d. 通信边界关联验证**

参考 `summary.md` 和 `arch.md`，确认该接口是否能与 arch.md 中的某个通信边界明确关联。无法关联 → **排除**。

#### 3.4.2 channel_read 深入分析

**a. 确认读方向**

区分信道的读和写：
- 只关心从信道**读取**外部数据的操作（读 = 外部输入）
- 向信道写入数据的操作不算（写 = 本模块输出，不构成外部输入）
- 如果函数同时有读取和写入，**保留**（只要存在读取操作）

**b. 单整数读取排除**

如果从信道仅读取一个整数数据（且非内存地址），→ **排除**。判断方式参考 3.4.1.c。

### 3.5 综合裁决

综合以上分析，给出最终结论：

| 结论 | 条件 |
|------|------|
| **排除** | 命中任一排除规则（快速排除 / 无外部输入 / 有内部调用者 / 仅单整数参数 / 仅单整数信道读取 / 无法关联通信边界） |
| **保留** | 未命中任何排除规则，有明确外部输入（参数或信道），与通信边界关联 |

### 3.6 记录结论

更新 `analysis_state.json` 中的条目并保存：

```json
{
  "index": 1,
  "name": "func_a",
  "file": "/abs/path/to/file.c",
  "status": "completed",
  "form": "parameter_input|channel_read|both",
  "decision": "keep|exclude",
  "reason": "<具体证据，不使用模糊描述>"
}
```

同时更新 `progress.json` 中 `api_analyzed` 和 `api_kept` 计数。

---

## 四、最终输出

全部接口分析完成后（`api_list` 中所有条目 status = `completed`），生成两个输出文件：

### api-clean.json

从 `analysis_state.json` 提取 `decision = "keep"` 的条目：

```json
[
  {"name": "func_a", "file": "/abs/path/to/a.c"},
  {"name": "func_c", "file": "/abs/path/to/c.c"}
]
```

格式与 api-finder 的 `api.json` 一致，便于后续工具链消费。

### summary.md

```markdown
# 接口去误报分析报告 — <project_dir>

## 分析概要
- 输入接口数: 15
- 保留: 8
- 排除: 7

## 保留接口

### 1. func_a — /abs/path/to/a.c
- **接口形式:** parameter_input
- **通信边界:** 用户态APP通过ioctl调用，data参数携带用户态缓冲区
- **外部输入:** data参数（void指针）来自copy_from_user，包含用户不可信数据
- **保留原因:** 明确的外部输入参数，与ioctl通信边界关联，无内部调用者

## 排除接口

### 3. func_b — /abs/path/to/b.c
- **排除原因:** 在 scope_files 中被 func_x 直接调用，属于内部传递，func_x 更适合作为外部接口
```

更新 `progress.json`：`phase = "done"`。告知用户分析完成。

---

## 五、约束规则

1. **所有分析由主 agent 完成**，不使用 sub-agent 并行
2. **分析质量优先于效率**，每个接口的源码必须认真阅读
3. **误报优先原则**：边界模糊时倾向排除
4. **reason 必须具体**：不使用"看起来像"、"可能是"等模糊描述
5. **逐接口保存状态**：每分析完一个接口立即写入 `analysis_state.json`，防止中断丢失
6. **文件绝对路径**：所有 file 字段使用绝对路径
7. **复用 api-finder 的双方法文件检查规则**（禁止 `test -f`）
8. **SKILL 全部提示词用中文**，相同语义用词保持一致
9. **适用业务**：嵌入式/底层系统代码（Linux 内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU 固件）

---

## 六、权限申请

| 工具 | 用途 |
|------|------|
| `Bash` | find 枚举文件、grep 搜索代码/调用关系、ls 检查文件、mkdir 创建目录 |
| `Read` | 读取源码、api.json、summary.md、arch.md、状态文件 |
| `Write` | 写入 progress.json、analysis_state.json、api-clean.json、summary.md |
| `mcp__codegraph__codegraph_explore` | (可选) 调用关系查询 |
| `mcp__plugin_oh-my-claudecode_t__ast_grep_search` | (可选) 代码模式搜索 |
