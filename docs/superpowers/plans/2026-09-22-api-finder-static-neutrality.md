# api-finder C 语法属性中立性 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 api-finder 明确"函数是否被 `static` 修饰、链接属性如何"既不构成保留依据也不构成排除依据，并消除提示词中与 C 关键字同形的英文 token。

**Architecture:** 纯文档修改，单文件 `api-finder/SKILL.md`。三处改动：§三 的注册模式枚举 `static|dynamic` 换名为 `sta_reg|dyn_reg`（避开 C 关键字），新增可增长的全局章节「八、注意事项」承载中立性声明（原「八、使用示例」顺延为「九、」），「抗理性化检查」表补一行。不改任何流程分支、判定条件与输出格式。

**Tech Stack:** Markdown

**Spec:** `docs/superpowers/specs/2026-09-22-api-finder-static-neutrality-design.md`

**验证方式：** 本改动是**预防性**的——api-finder 尚未观察到 static 误排实例，无 bug 可复现。验证分两层：

1. **文案层（主）：** 核对英文 token `static`/`dynamic` 已从提示词中清除（新增声明除外）；「注意事项」首句声明了全局适用；其内容与 §五 规则二（无外部输入排除）不矛盾；中文「静态注册/动态注册」8 处未被误伤。
2. **回归层（辅）：** 实跑一次 `/api-finder test_fixtures/project`，确认 `api.json` 仍为 7 条且与基线逐条一致。

---

### Task 1: §三 注册模式枚举换词

**Files:**
- Modify: `api-finder/SKILL.md:267`、`:276`、`:286`、`:305`

**背景：** 这 4 处的 `static`/`dynamic` 是注册模式的枚举值（编译期填入全局表 / 运行时调用注册函数），与 C 的存储类说明符无关。同名 token 是潜在混淆源，换成语义等价且不与 C 关键字冲突的缩写。

- [ ] **Step 1: 四处枚举值换词**

将（267 行，4 个前导空格）：

```
    "type": "static|dynamic",
```

替换为：

```
    "type": "sta_reg|dyn_reg",
```

将（276 行，2 个前导空格）：

```
  "type": "static",
```

替换为：

```
  "type": "sta_reg",
```

将（286 行，2 个前导空格）：

```
  "type": "dynamic",
```

替换为：

```
  "type": "dyn_reg",
```

将（305 行，3 个前导空格）：

```
   - 两个 pattern 同类型（都是 static 或都是 dynamic）且共享相同的注册载体（同一个 `var_type` + `var_name`，或同一个 `register_func`）→ 合并
```

替换为：

```
   - 两个 pattern 同类型（都是 sta_reg 或都是 dyn_reg）且共享相同的注册载体（同一个 `var_type` + `var_name`，或同一个 `register_func`）→ 合并
```

276 行（静态注册的 details 示例）与 286 行（动态注册的 details 示例）必须同时改，否则两个示例不对称。

- [ ] **Step 2: 验证并提交**

用 Read 重读 `api-finder/SKILL.md` 的 263-292 行与 299-310 行。核对点：

- 四处枚举值已换为 `sta_reg` / `dyn_reg`，两处 details 示例的字段（`var_type`/`var_name`/`binding_field` 与 `register_func`/`callback_param_index`）未被改动
- `grep -n 'static\|dynamic' api-finder/SKILL.md` **无输出**（此时「注意事项」尚未添加，全文不应再有这两个英文词）
- `grep -n '静态注册\|动态注册' api-finder/SKILL.md` 仍为 8 行命中（246/247/254/256/273/283/326/328），中文术语未被误伤
- `git diff --stat` 仅 `api-finder/SKILL.md` 一个文件，且为 4 增 4 删

```bash
git add api-finder/SKILL.md
git commit -m "docs(api-finder): rename the registration-type enum off the C keyword"
```

---

### Task 2: 新增「八、注意事项」并同步抗理性化表

**Files:**
- Modify: `api-finder/SKILL.md:713-733`（§七 约束规则 末尾至原「八、使用示例」）
- Modify: `api-finder/SKILL.md`（「抗理性化检查」表末）

- [ ] **Step 1: 插入「八、注意事项」章节，原「八、使用示例」改为「九、」**

将：

```
6. **文件绝对路径。** api.json、中间结果 JSON 中的 file 字段一律使用绝对路径。

---

## 八、使用示例
```

替换为：

```
6. **文件绝对路径。** api.json、中间结果 JSON 中的 file 字段一律使用绝对路径。

---

## 八、注意事项

以下事项适用于全部阶段，无论在哪一步做判断都成立。

1. **C 语法属性不参与接口判定。** 函数是否被 `static` 修饰、链接属性是内部链接还是外部链接，既不构成保留依据，也不构成排除依据——不因被 `static` 修饰而排除，也不因其未被 `static` 修饰而认定。判定只依据通信边界、数据来源与注册方式。

---

## 九、使用示例
```

新增条目写成**单行**（不折行），与 §七 约束规则各条的排版一致。已有的 `---` 保留在新章节之前，新章节与「九、使用示例」之间补一个 `---`。

- [ ] **Step 2: 「抗理性化检查」表追加一行**

将：

```
| "这个状态文件检查结果不太确定，先继续吧" | 文件存在性检查必须严格遵循双方法+判断规则。 |
```

替换为：

```
| "这个状态文件检查结果不太确定，先继续吧" | 文件存在性检查必须严格遵循双方法+判断规则。 |
| "这个函数被 static 修饰，不像外部接口" | C 语法属性不参与判定，被 static 修饰既不构成排除依据，也不构成保留依据。 |
```

该行原为文件最后一行，追加后仍为最后一行。

- [ ] **Step 3: 验证并提交**

用 Read 重读 `api-finder/SKILL.md` 的 727-745 行与文件末尾。核对点：

- 「八、注意事项」位于 §七 约束规则之后、「九、使用示例」之前；两节之间均有 `---` 分隔
- 注意事项正文为：引导句「以下事项适用于全部阶段，无论在哪一步做判断都成立。」+ 单条编号 1
- 「九、使用示例」下辖的三个子标题（首次分析 / 断点续分析 / 迭代分析）内容未变
- `grep -n '^## ' api-finder/SKILL.md` 显示章节顺序为 ……六、权限申请 → 七、约束规则 → 八、注意事项 → 九、使用示例 → 抗理性化检查
- `grep -n 'static' api-finder/SKILL.md` 输出**恰为 2 行**：注意事项第 1 条、抗理性化表新行；`grep -n 'dynamic' api-finder/SKILL.md` **无输出**
- 抗理性化表新行为 2 列，与既有 6 行格式一致
- 全文无按编号的交叉引用需要同步（`grep -n '第[一二三四五六七八九十]节' api-finder/SKILL.md` 无输出）

```bash
git add api-finder/SKILL.md
git commit -m "docs(api-finder): add a global notes section on syntax-attribute neutrality"
```

---

### Task 3: 全文一致性核对与回归验证

**Files:**
- Review: `api-finder/SKILL.md`（全文）

- [ ] **Step 1: 重读全文，逐项核对**

用 Read 通读整个 `api-finder/SKILL.md`（约 840 行），核对：

1. **中立性声明无歧义。** 「八、注意事项」第 1 条同时排除了"加分"与"减分"两种误用（既不构成保留依据，也不构成排除依据），且未出现"推荐保留 static 函数"之类的反向表述。
2. **与 §五 规则二不矛盾。** §五 规则二（无外部输入排除，核心规则）判的是"函数是否接收来自外部模块的数据"；注意事项判的是"C 语法属性是否影响判定"。确认两处维度不同、互不冲突——即：注意事项没有为任何函数提供免于 §五 审查的豁免。
3. **与「核心原则」第 2 条（更少的误报优先于更少的漏报）不矛盾。** 该原则适用于"是否为外部接口"证据不足时的取舍；注意事项取消的是"C 语法属性"这一本不该参与判定的维度，不是放宽证据标准。
4. **枚举换词已贯通。** §三 的 schema 占位、静态注册示例、动态注册示例、去重比较四处取值一致；中文「静态注册/动态注册」与新枚举的对应关系读者可自行建立（无需额外映射说明）。
5. **无残留。** 全文不再出现英文 `dynamic`；英文 `static` 仅出现在新增的注意事项与抗理性化行中。

如发现不一致，直接修复并重新核对。

- [ ] **Step 2: 回归实跑**

按计划末尾「验收标准」执行。

- [ ] **Step 3: 最终提交**

```bash
git add api-finder/SKILL.md
git commit -m "docs(api-finder): final consistency pass for syntax-attribute neutrality"
```

（若 Step 1 无改动，跳过本次提交，报告即可。spec `docs/superpowers/specs/2026-09-22-api-finder-static-neutrality-design.md` 已在 brainstorming 阶段提交，本次不重复提交。）

---

## 验收标准（全部任务完成后）

对 `test_fixtures/project` 实跑一次 `/api-finder`，确认未引入回归：

1. 备份基线：
   ```bash
   cp test_fixtures/project/.ethunter_out/api-finder/api.json /tmp/api-finder-api.baseline.json
   cp test_fixtures/project/.ethunter_out/api-finder/progress.json /tmp/api-finder-progress.baseline.json
   ```
2. 删除 `test_fixtures/project/.ethunter_out/api-finder/progress.json`。**只删这一个文件即可**：fixture 的 `tmp/` 下只有 `arch_apis.json` 与 `feature_apis.json`，`progress.json` 引用的 `tmp/feature_state.json`、`tmp/arch_identify_state.json`、`tmp/filter_state.json` 均不存在，故删除后 skill 会走全新分析路径。`arch.md`（`api-archreader/` 与 `api-finder/` 各一份）保留，是 §四 的必需输入。
3. 派子代理按修改后的 `api-finder/SKILL.md` 执行 `/api-finder test_fixtures/project`。子代理须完整读取 SKILL.md 后按其流程执行，且**不得读取** `test_fixtures/TEST_PLAN.md`、`docs/superpowers/`；`api.json` 会在运行中被覆盖，属预期。
4. 比对实跑产出的 `api.json` 与 `/tmp/api-finder-api.baseline.json`，**逐条目 `name` + `file` 一致**。基线为 7 条：
   ```
   handler_func_b / process_ipc_message / read_from_shared_memory / handle_ipc_queue
   / handle_system_event / dispatch_ioctl / read_device_status
   ```
5. 额外确认 feature 阶段未因枚举换词而失效：若 `tmp/feature_apis.json` 在本次运行后存在，其内容应与基线一致（若该文件本轮未被写出，以第 4 步的 `api.json` 比对为准即可，不作为失败条件）。
6. 比对通过后把实跑产出恢复到基线（`git checkout -- <文件>`），避免留下格式噪音。

**本次期望结果：与基线零差异。** 本改动不改变任何识别判定。若出现差异，先判定是模型执行偏差还是文案缺陷——前者重跑（可提高子代理能力档位），后者回到 Task 3 Step 1 修复文案；**不得把差异直接改基线迁就**。

**复现性说明：** 实跑通过**不构成**"static 误排已被修复"的证据，本 skill 本来就没观察到该问题。回归只证明未引入破坏；中立性效果的直接依据是 Task 3 Step 1 的文案核对。
