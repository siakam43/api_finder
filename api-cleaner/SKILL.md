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
