# api-cleaner 新增通信边界与参数索引字段 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** api_clean.json 输出从 4 字段扩为 7 字段，新增 `local_module`（本模块名称）、`partner_module`（交互模块名称）、`para_index`（外部输入参数 0-based 索引，与 taint_data 逐项对应）。

**Architecture:** 纯文档修改，单文件 `api-cleaner/SKILL.md`。字段随分析步骤就地记录：初始化定 local_module，2.4.b 记参数索引，2.4.d / 新增 2.5.c 记 partner_module，2.7 组装 para_index，4.1/4.2 输出。不改任何代码逻辑或工具，不改 api-archreader。

**Tech Stack:** Markdown

**Spec:** `docs/superpowers/specs/2026-09-10-api-cleaner-boundary-fields-design.md`

**验证方式：** 每个任务编辑后用 Read 重读被改小节，逐项核对下方"核对点"；无单元测试可跑。

---

### Task 1: Step 6 初始化 — 确定 local_module 并扩展 entry 结构

**Files:**
- Modify: `api-cleaner/SKILL.md` Step 6（约 111-167 行）

- [ ] **Step 1: 在 Step 6 中插入"确定本模块名称"步骤**

在 Step 6 第 1 点（读取 api.json 及其 json 示例代码块）之后、"2. 用两种方法检查 progress.json"之前，插入新第 2 点，并将原第 2、3 点重新编号为 3、4。

插入文本：

```markdown
2. 确定本模块名称（local_module）：读取 `<project_dir>/.ethunter_out/api-archreader/arch.md` 的"模块概要"章节，提取简洁模块名（如 `ISP 固件`、`SensorHub 固件`）。该值在本次分析中所有接口共用。
```

同时把原有：

```markdown
2. 用两种方法检查 `<project_dir>/.ethunter_out/api-cleaner/progress.json` 是否存在：
```

改为：

```markdown
3. 用两种方法检查 `<project_dir>/.ethunter_out/api-cleaner/progress.json` 是否存在：
```

把原有：

```markdown
3. 更新 progress.json 中 `phase = "analyzing"`，保存。
```

改为：

```markdown
4. 更新 progress.json 中 `phase = "analyzing"`，保存。
```

- [ ] **Step 2: 扩展 analysis_state.json 结构示例**

将结构示例中：

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

替换为：

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
      "taint_data": null,
      "local_module": null,
      "partner_module": null,
      "para_index": null
    }
  ]
}
```

- [ ] **Step 3: 更新全新分析初始化规则**

将：

```markdown
- `api_list` 中每个条目：index 递增（1-based），status = `"pending"`，form/decision/reason 均为 null
```

替换为：

```markdown
- `api_list` 中每个条目：index 递增（1-based），status = `"pending"`，form/decision/reason/taint_data/partner_module/para_index 均为 null，local_module = 第 2 步确定的本模块名称（所有条目同一值）
```

- [ ] **Step 4: 验证并提交**

用 Read 重读 Step 6 整节。核对点：
- 编号为 1、2（本模块名称）、3（progress 检查）、4（phase 更新），无重复或遗漏
- entry 结构含全部 9 个字段，新 3 字段为 null
- 初始化规则中 local_module 为第 2 步的值，partner_module/para_index 为 null

```bash
git add api-cleaner/SKILL.md
git commit -m "docs(api-cleaner): init local_module from arch.md and extend analysis_state schema"
```

---

### Task 2: 2.4.b — 记录外部输入参数索引

**Files:**
- Modify: `api-cleaner/SKILL.md` 2.4.b（约 279-293 行）

- [ ] **Step 1: 替换 2.4.b 整节**

将：

```markdown
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
```

替换为：

```markdown
#### b. 锁定外部输入参数列表

对函数签名中的每个入参逐一分析，筛选出外部输入参数。

**先为签名全部参数编号：** 从 0 开始（第一个参数索引为 0），框架参数、输出参数也占位。例如 `func(int fd, void *data, int len)` 中：fd = 0、data = 1、len = 2。

对每个参数回答以下两个问题：
1. 该参数是系统框架类参数吗？（`struct file *f`、`struct inode *i`、内核内部数据结构指针、框架回调约定的上下文参数等）→ 是 → **内部参数，跳过**
2. 该参数用于输出目的吗？（`void *out_buffer`、`int *result`、`void *ret_data`），代码中对该参数仅写入不读取 → 是 → **内部参数，跳过**

两个都否 → 该参数为**外部输入参数**，加入外部输入参数列表，并记录该参数的 0-based 索引。

统计外部输入参数列表：
- 个数 = 0 → **直接排除**（无外部输入），记录 reason
- 个数 > 0 → 记录参数名到 `taint_data` 字段（多个以 `/` 分割），按相同顺序记录各参数索引（多个以 `/` 分割），供 2.7 组装 `para_index`，进入 c. 外部输入参数个数与类型裁决

`taint_data` 示例：`"data/len"`（两个参数都是外部输入）、`"buf"`（仅 buf 是外部输入）。
```

- [ ] **Step 2: 验证并提交**

用 Read 重读 2.4.b。核对点：
- 含"从 0 开始编号、框架/输出参数占位"的规则
- 外部输入参数同时记录 0-based 索引
- 索引记录顺序与 taint_data 参数名顺序一致

```bash
git add api-cleaner/SKILL.md
git commit -m "docs(api-cleaner): record 0-based param index in 2.4.b"
```

---

### Task 3: 2.4.d — 关联成功后记录 partner_module

**Files:**
- Modify: `api-cleaner/SKILL.md` 2.4.d（约 317-325 行）

- [ ] **Step 1: 在 2.4.d 末尾追加记录规则**

将：

```markdown
**无法与任何通信边界明确关联 → 排除。**
```

（2.4.d 小节中的这一处，注意与 Task 4 中新 2.5.c 区分，此时 2.5.c 尚不存在，唯一匹配）

替换为：

```markdown
**无法与任何通信边界明确关联 → 排除。**

确认关联后：从边界表对应行的"外部实体"列取值，记录到 `partner_module` 字段。若同时与多条边界关联，多个外部实体名以 `/` 分割（如 `"用户态 APP/DDR 控制器"`）。
```

- [ ] **Step 2: 验证并提交**

用 Read 重读 2.4.d。核对点：
- 原排除逻辑保留
- 新增"外部实体列取值 → partner_module、多条 / 分割"规则

```bash
git add api-cleaner/SKILL.md
git commit -m "docs(api-cleaner): record partner_module in 2.4.d boundary association"
```

---

### Task 4: 2.5 — 新增 c 小节（通信边界关联验证）

**Files:**
- Modify: `api-cleaner/SKILL.md` 2.5（在 b 小节之后、2.6 之前追加，约 352 行后）

- [ ] **Step 1: 在 2.5.b 之后追加 2.5.c**

在 2.5.b 的末尾文本：

```markdown
判断方式参考 2.4.c 中的内存地址判断规则（整数强转为指针、用于内存操作 → 保留）。
```

之后追加：

```markdown
#### c. 通信边界关联验证

参考 `<project_dir>/.ethunter_out/api-finder/finder_summary.md` 中该接口的识别理由，以及 `<project_dir>/.ethunter_out/api-archreader/arch.md` 中的"外部通信边界"表。

确认该接口是否能与 arch.md 中的某个通信边界明确关联：
- 该接口与哪个外部模块/实体通信？
- 信道读取的数据是否与该通信信道的数据格式匹配？

**无法与任何通信边界明确关联 → 排除。**

确认关联后：从边界表对应行的"外部实体"列取值，记录到 `partner_module` 字段。若同时与多条边界关联，多个外部实体名以 `/` 分割（如 `"用户态 APP/DDR 控制器"`）。

信道来源不是函数参数，信道项在 `para_index` 中对应 `-1`。
```

- [ ] **Step 2: 验证并提交**

用 Read 重读 2.5 整节。核对点：
- 小节顺序为 a、b、c
- c 的关联验证逻辑与 2.4.d 一致（同一段规则文字）
- c 含"信道项 para_index = -1"说明

```bash
git add api-cleaner/SKILL.md
git commit -m "docs(api-cleaner): add 2.5.c boundary association for channel_read"
```

---

### Task 5: 2.7 — 组装 para_index、合并 partner_module、写入新字段

**Files:**
- Modify: `api-cleaner/SKILL.md` 2.7（约 365-391 行）

- [ ] **Step 1: 在 2.7 开头插入组装规则**

将：

```markdown
更新 `analysis_state.json` 中当前接口的条目：
```

替换为：

```markdown
**组装新字段（仅 decision = `"keep"` 的接口执行；排除的接口 partner_module/para_index 保持 null）：**

1. 组装 `para_index`：按 `taint_data` 项顺序逐项对应（form = `both` 时参数项在前、信道项在后）：
   - 参数项 → 该参数在函数签名中的 0-based 索引（2.4.b 记录）
   - 信道项 → `-1`
   - 多项以 `/` 分割；单项也是字符串（如 `"0"`、`"-1"`）
   - 项数必须与 `taint_data` 项数一致，组装后逐项核对

2. form = `both` 时合并 `partner_module`：将 2.4.d 与 2.5.c 的关联结果去重合并，多个外部实体名以 `/` 分割。

更新 `analysis_state.json` 中当前接口的条目：
```

- [ ] **Step 2: 更新 entry JSON 示例**

将：

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

替换为：

```json
{
  "index": 1,
  "name": "func_a",
  "file": "/abs/path/to/file.c",
  "status": "completed",
  "form": "parameter_input|channel_read|both|none",
  "decision": "keep|exclude",
  "reason": "<具体证据，不使用模糊描述>",
  "taint_data": "<外部输入变量名或来源>",
  "local_module": "<本模块名称，初始化时已写入>",
  "partner_module": "<交互模块名称，多个以 / 分割；排除接口为 null>",
  "para_index": "<与 taint_data 逐项对应的参数索引，多个以 / 分割，信道项为 -1；排除接口为 null>"
}
```

- [ ] **Step 3: 验证并提交**

用 Read 重读 2.7。核对点：
- 组装规则在 entry 更新之前
- para_index 规则：逐项对应、参数项 0-based、信道项 -1、/ 分割、项数一致
- both 时 partner 去重合并
- entry JSON 含 3 个新字段及说明

```bash
git add api-cleaner/SKILL.md
git commit -m "docs(api-cleaner): assemble para_index and merge partner_module in 2.7"
```

---

### Task 6: 4.1 — api_clean.json 输出 7 字段

**Files:**
- Modify: `api-cleaner/SKILL.md` 4.1（约 415-435 行）

- [ ] **Step 1: 更新字段表**

将：

```markdown
每个条目包含以下 **4 个字段**，直接从 `analysis_state.json` 对应条目取值：

| 字段 | 来源 | 说明 |
|------|------|------|
| `name` | `analysis_state.json` 的 `name` | 函数名 |
| `file` | `analysis_state.json` 的 `file` | 文件绝对路径 |
| `form` | `analysis_state.json` 的 `form` | `parameter_input` / `channel_read` / `both` |
| `taint_data` | `analysis_state.json` 的 `taint_data` | 外部输入变量名或来源 |
```

替换为：

```markdown
每个条目包含以下 **7 个字段**，直接从 `analysis_state.json` 对应条目取值：

| 字段 | 来源 | 说明 |
|------|------|------|
| `name` | `analysis_state.json` 的 `name` | 函数名 |
| `file` | `analysis_state.json` 的 `file` | 文件绝对路径 |
| `form` | `analysis_state.json` 的 `form` | `parameter_input` / `channel_read` / `both` |
| `taint_data` | `analysis_state.json` 的 `taint_data` | 外部输入变量名或来源 |
| `local_module` | `analysis_state.json` 的 `local_module` | 本模块名称 |
| `partner_module` | `analysis_state.json` 的 `partner_module` | 交互模块名称，多个以 `/` 分割 |
| `para_index` | `analysis_state.json` 的 `para_index` | 与 taint_data 逐项对应的参数索引，多个以 `/` 分割，信道项为 -1 |
```

- [ ] **Step 2: 更新输出示例 JSON**

将：

```json
[
  {"name": "func_a", "file": "/abs/path/to/a.c", "form": "parameter_input", "taint_data": "data/len"},
  {"name": "func_c", "file": "/abs/path/to/c.c", "form": "channel_read", "taint_data": "共享内存 shm_ptr"}
]
```

替换为：

```json
[
  {"name": "func_a", "file": "/abs/path/to/a.c", "form": "parameter_input", "taint_data": "data/len",
   "local_module": "ISP 固件", "partner_module": "用户态 APP", "para_index": "1/2"},
  {"name": "func_c", "file": "/abs/path/to/c.c", "form": "channel_read", "taint_data": "共享内存 shm_ptr",
   "local_module": "ISP 固件", "partner_module": "DDR 控制器", "para_index": "-1"},
  {"name": "func_d", "file": "/abs/path/to/d.c", "form": "both", "taint_data": "buf/共享内存 shm_ptr",
   "local_module": "ISP 固件", "partner_module": "用户态 APP/DDR 控制器", "para_index": "0/-1"}
]
```

- [ ] **Step 3: 验证并提交**

用 Read 重读 4.1。核对点：
- "7 个字段"，表含全部 7 行
- 示例覆盖三种 form，para_index 与 taint_data 项数对齐，channel_read 为 "-1"
- "其余字段不输出"一行仍正确

```bash
git add api-cleaner/SKILL.md
git commit -m "docs(api-cleaner): output 7 fields in api_clean.json"
```

---

### Task 7: 4.2 — cleaner_summary.md 通信边界行使用新字段

**Files:**
- Modify: `api-cleaner/SKILL.md` 4.2（约 449-455 行）

- [ ] **Step 1: 更新保留接口模板中的通信边界行**

将：

```markdown
- **通信边界:** <该接口与哪个外部模块通信，通过什么信道>
```

替换为：

```markdown
- **通信边界:** <local_module> → <partner_module>，通过<信道名>
```

- [ ] **Step 2: 验证并提交**

用 Read 重读 4.2。核对点：通信边界行引用记录的两个新字段。

```bash
git add api-cleaner/SKILL.md
git commit -m "docs(api-cleaner): use recorded boundary fields in cleaner_summary"
```

---

### Task 8: 约束规则 — 新字段称谓一致性

**Files:**
- Modify: `api-cleaner/SKILL.md` 约束规则第 8 条（约 491 行）

- [ ] **Step 1: 扩展约束规则第 8 条**

将：

```markdown
8. **SKILL 全部提示词使用中文。** 相同语义的用词前后保持一致。
```

替换为：

```markdown
8. **SKILL 全部提示词使用中文。** 相同语义的用词前后保持一致。新字段统一称谓：本模块名称（local_module）、交互模块名称（partner_module）、参数索引（para_index）；参数索引为 0-based，第一个参数索引为 0。
```

- [ ] **Step 2: 验证并提交**

用 Read 重读约束规则第 8 条。核对点：三个新字段的中文称谓与全文一致（2.4.b/2.5.c/2.7/4.1 使用相同称谓）。

```bash
git add api-cleaner/SKILL.md
git commit -m "docs(api-cleaner): standardize new field terminology in constraints"
```

---

### Task 9: 全文件一致性检查与最终提交

**Files:**
- Review: `api-cleaner/SKILL.md`（全文）

- [ ] **Step 1: 重读全文，逐项核对 Spec**

用 Read 通读整个 SKILL.md，按以下清单核对（对应 spec `docs/superpowers/specs/2026-09-10-api-cleaner-boundary-fields-design.md`）：

1. Step 6：local_module 提取自 arch.md 模块概要，entry 含 9 字段
2. 2.4.b：0-based 索引记录规则存在
3. 2.4.d：排除逻辑保留 + partner_module 记录
4. 2.5.c：与 2.4.d 相同的关联验证，信道项 -1
5. 2.7：para_index 组装规则（逐项对应、/ 分割、项数一致）、both 时 partner 去重合并、排除接口不写新字段
6. 4.1：7 字段表 + 三种 form 示例
7. 4.2：通信边界行使用 local_module → partner_module
8. 约束规则 8：新字段称谓统一
9. 全文无残留 "4 个字段" 旧表述；无与本次修改矛盾的段落

如发现不一致，直接修复并重新核对。

- [ ] **Step 2: 最终提交**

```bash
git add api-cleaner/SKILL.md
git commit -m "docs(api-cleaner): final consistency pass for boundary fields"
```

（若 Step 1 无改动，跳过本次提交，报告即可。）

---

## 验收标准（全部任务完成后）

对任一测试项目实际运行 `/api-cleaner`，检查 `api_clean.json`：
- 每条 keep 记录含 7 个字段，顺序为 name/file/form/taint_data/local_module/partner_module/para_index
- 所有记录 local_module 值相同
- para_index 与 taint_data 逐项对齐：参数项为 0-based 索引，信道项为 -1
- channel_read 保留接口均经过边界关联验证（2.5.c），partner_module 非空
