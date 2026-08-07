# api-fixer 设计

## 概述

api-fixer 是一个独立 markdown skill（`api-fixer/SKILL.md`），消费历史版本中识别出的外部接口列表（old_api.json），修复其中的路径错误，排除已删除的接口，输出一个正确的外部接口列表。

风格与 api-finder、api-cleaner 保持一致。适用业务：嵌入式/底层系统代码（Linux 内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU 固件）。

## 使用方法

```
/api-fixer <project_dir>
```

未指定 `project_dir` 时默认为当前工作目录。

## 输出

```
.ethunter_out/api-fixer/
├── inherited_apis.json   # 修复后的外部接口列表
└── progress.json         # 断点续分析状态 + 逐条目处理结果
```

无需 `conf/` 和 `tmp/` 子目录。

## 整体章节结构

| 章节 | 内容 | 来源 |
|------|------|------|
| 核心原则 | 分析质量优先、误报优先 | api-finder |
| 使用方法 | `/api-fixer <project_dir>` | rr3.md |
| 输出 | inherited_apis.json + progress.json | rr3.md |
| 一、初始化与分析范围确定 | scope_files + codegraph + old_api.json 校验 + 输出目录创建 | api-finder §1 + api-cleaner 报错逻辑 |
| 二、任务恢复机制 | 双方法交叉确认 + progress.json 结构 + 恢复流程 | api-finder §2 |
| 三、外部接口列表修复 | 完整复制 api-finder §4（2b→2c→2d 流程） | api-finder §4 |
| 四、约束规则 | 主 agent 完成、中文提示词等 | rr3.md |
| 五、权限申请 | Bash / Read / Write / codegraph | api-finder §8 |
| 六、抗理性化检查 | STOP 检查表 | api-finder §9 |

## 一、初始化与分析范围确定

**Step 1: 解析输入参数** — 和 api-finder 完全一致。

**Step 2: 确定代码分析范围（scope_files）** — 和 api-finder §1 Step 2 完全一致（clean_code.txt → .etignore → find 全量 .c/.h）。

**Step 3: 检查 old_api.json** — **必须存在**，不存在则报错退出（与 api-cleaner 缺输入行为一致）。

- old_api.json 路径：`<project_dir>/.ethunter_out/old_api.json`
- 格式：`[{"name": "FUNC_NAME", "file": "FILE_PATH"}, ...]`
- `file` 可能是绝对路径或相对路径

**Step 4: 检测 CodeGraph** — 和 api-finder §1 Step 4 一致。

**Step 5: 初始化输出目录** — `mkdir -p <project_dir>/.ethunter_out/api-fixer`

## 二、任务恢复机制

### 文件存在性检查规则

复用 api-finder §2 双方法交叉确认规则（ls + Read + find），禁止 `test -f`。

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

`results` 条目格式与 api-finder 的 inherit_result.json 完全一致。

### 入口恢复流程

```
1. 检查 progress.json 是否存在。
   ├── 确认不存在 → 全新分析，初始化 progress.json
   └── 确认存在 → 加载 progress.json

2. 遍历 old_api.json 每个条目，将 name + file 与 progress.results 中
   name + original_file 对比：
   ├── 已在 results 中 → 跳过
   └── 不在 results 中 → 待处理条目

3. 待处理条目为空 → 跳到输出结果
   待处理条目非空 → 继续第三节
```

## 三、外部接口列表修复

核心逻辑完整复制 api-finder §4。

### 搜索函数定义的方法

两步法：grep 定位 → Read 验证（完整签名 + `{` 允许换行、不以 `;` 结尾、不在注释中）。

### 流程

**逐条处理每个待处理条目：**

**Step 2a — 路径规范化：** 将 `file` 规范化为绝对路径，记录 `original_file` = 原始值。

**Step 2b — 路径范围检查：** 路径在 scope_files 中 → 继续 2c；不在 → 直接进 2d。

**Step 2c — 函数存在性检查：** 在 `file` 中用两步法确认函数定义存在。
- 存在 → `result = "inherited"`, `path_updated = false`, `reason = null`
- 不存在 → 进入 Step 2d

**Step 2d — fallback 搜索：**
- 检查 results 中是否已有同名且 result = "inherited" 的条目 → 有则淘汰
- 无同名继承 → 在 scope_files 中搜索（两步法），只保留路径在 scope_files 中的结果
  - 找到 → 更新 file，`path_updated = true`, `result = "inherited"`
  - 未找到 → `result = "eliminated"`

2d 不再回链任何步骤。

**每处理完一条立即保存 progress.json。**

### 输出结果

- 将 `result = "inherited"` 的条目（仅 name + file 字段，file 使用最终路径）写入 `inherited_apis.json`
- 更新 progress.json：`phase = "done"`

## 四、约束规则

1. 所有分析由主 agent 完成，不使用 sub-agent 并行分析
2. 分析质量优先于分析效率
3. 严格遵循去重规则
4. 按设计流程的指定顺序执行
5. 文件绝对路径。inherited_apis.json 中的 file 字段使用绝对路径
6. 整个 skill 提示词用中文，相同语义用词前后保持一致
7. 适用业务：嵌入式/底层系统代码
8. 文件存在性检查严格遵循双方法+判断规则，禁止使用 `test -f`

## 五、权限申请

| 工具 | 用途 |
|------|------|
| `Bash` | find 枚举文件、grep 搜索代码、ls 检查文件、mkdir 创建目录 |
| `Read` | 读取源码、old_api.json、progress.json |
| `Write` | 写入 progress.json、inherited_apis.json |
| `mcp__codegraph__codegraph_explore` | (可选) codegraph 可用时优先使用 |

codegraph 不可用时使用常规工具，不中断分析。

## 六、抗理性化检查

| 想法 | 现实 |
|------|------|
| "条目太多了，我加快一点" | 分析质量优先于效率。每个条目必须认真验证。 |
| "这个函数名很常见，跳过 fallback 吧" | 必须完整执行 2b→2c→2d 流程。 |
| "这个状态文件检查结果不太确定，先继续吧" | 文件存在性检查必须严格遵循双方法+判断规则。 |

## 与 api-finder 的关键差异

- **无架构分析阶段**（不需要 arch.md），只需 scope_files
- **无特征提取/架构识别/筛选阶段**（api-finder §5/§6/§7），只做 inherit
- **old_api.json 不存在时报错退出**，而非跳过
- **输出目录**：`api-fixer/` 替代 `api-finder/`
- **progress.json** 同时承载断点恢复 + 逐条目处理结果（不另设 inherit_result.json，无 tmp/ 子目录）
