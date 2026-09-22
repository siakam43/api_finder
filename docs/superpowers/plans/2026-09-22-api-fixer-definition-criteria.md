# api-fixer 函数定义判定范围 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除模型在两步法四条判定标准之外自行追加"链接属性"条件的空间——被 `static` 修饰的函数体定义必须判为"找到定义"。

**Architecture:** 纯文档修改，单文件 `api-fixer/SKILL.md`。不改任何流程分支：a→b→c→d 的进入条件、去重规则、reason 模板取值全部不变，只补两处文案——§三 两步法判定标准后加一条通用原则，§六 抗理性化表加一行。原则以「链接属性（内部链接 / 外部链接）」为上位概念，`static` 仅作例子，避免让 `static` 抢戏。

**Tech Stack:** Markdown

**Spec:** `docs/superpowers/specs/2026-09-22-api-fixer-definition-criteria-design.md`

**验证方式：** 本缺陷非必现（实测出现过、重跑即消失），**无法用重跑做可靠的复现验证**。验证分两层：

1. **文案层（主）：** 核对新增文字无歧义地禁止了"四条之外的第五条判定标准"，并确认全文无可被解读为"链接属性参与判定"的表述。
2. **回归层（辅）：** 实跑一次 `/api-fixer test_fixtures/project`，确认 10 条判定与 tag 分布与 fixture 完全一致——本次改动不应改变任何判定结果。

---

### Task 1: §三 判定标准处补原则 + §六 追加抗理性化行

**Files:**
- Modify: `api-fixer/SKILL.md:196`（§三「搜索函数定义的方法」判定结论句之后）
- Modify: `api-fixer/SKILL.md:302`（§六 抗理性化检查表末行之后）

- [ ] **Step 1: 在两步法判定结论句之后补通用原则**

将（注意 `全部满足` 一行有 **3 个前导空格**；带上后面的 `### 流程` 以便唯一定位）：

```
   全部满足 → 确认存在函数体定义。任一不满足 → 不是定义，继续检查下一个候选。所有候选都不满足 → 未找到。

### 流程
```

替换为：

```
   全部满足 → 确认存在函数体定义。任一不满足 → 不是定义，继续检查下一个候选。所有候选都不满足 → 未找到。

**判定只看上述四条语法特征。** 函数是否被 `static` 修饰、其链接属性为内部链接还是外部链接、
是否对外可见、是否被其他模块调用，都不参与判定——**被 `static` 修饰的函数体定义同样是函数体定义。**
本阶段只确认函数体定义是否存在，不重新判定接口是否对外暴露。

### 流程
```

新增段落从行首开始（不缩进），作为该小节的收尾段落，而非 `2. Read 验证` 列表项的一部分。

- [ ] **Step 2: §六 抗理性化检查表追加一行**

将：

```
| "reason 随便写一句就行" | 必须使用 [c1]/[d1]/[d2]/[d3] 四种 tag 之一，文案逐字采用第三节 c、d 分支的模板。 |
```

替换为：

```
| "reason 随便写一句就行" | 必须使用 [c1]/[d1]/[d2]/[d3] 四种 tag 之一，文案逐字采用第三节 c、d 分支的模板。 |
| "这个函数被 static 修饰，不像外部接口" | 本阶段只确认函数体定义是否存在；被 static 修饰的函数体定义同样是定义，不影响判定。 |
```

- [ ] **Step 3: 验证并提交**

用 Read 重读 `api-fixer/SKILL.md` 的 185-203 行与 291-306 行。核对点：

- 新增原则段位于 196 行结论句之后、`### 流程` 之前，行首无缩进
- 四条语法特征 bullet（192-195 行：完整函数签名 / 签名后有 `{` / 不以 `;` 结尾 / 不在注释中）逐字未变
- §六 新行紧随 `"reason 随便写一句就行"` 那行之后，且该行位于表格最末（其后是空行与 `---`）
- §六 表格每行仍为 2 列，各行 `|` 数量一致
- `grep -n 'static' api-fixer/SKILL.md` 输出恰为 3 行，且全部落在本次新增的文本内（改动一占 2 行，改动二占 1 行）；改动前该命令为零命中
- `git diff --stat` 仅 `api-fixer/SKILL.md` 一个文件

```bash
git add api-fixer/SKILL.md
git commit -m "docs(api-fixer): scope definition judgment to syntax, not linkage"
```

---

### Task 2: 全文一致性核对与回归验证

**Files:**
- Review: `api-fixer/SKILL.md`（全文）

- [ ] **Step 1: 重读全文，确认无表述与新增原则冲突**

用 Read 通读整个 `api-fixer/SKILL.md`，重点核对：

1. **无第五条标准的遐想空间。** 通读后确认全文没有任何表述可被解读为"链接属性 / 可见性 / 对外暴露参与定义判定"。特别检查：
   - §三 两步法（191-203 行）：`是否满足以下**全部条件**` 的措辞现在明确只有四条
   - §三 流程 c/d（219-242 行）：两个判定点都调用"搜索函数定义的方法"，故都受新原则覆盖；确认 c/d 自身没有追加条件
   - §四 约束规则：确认无涉及链接属性的条目
   - §七 使用示例：确认示例不暗示 static 相关判定
2. **§六 的"不确定时倾向淘汰"（299 行）与新行的关系。** 该原则用于"函数定义是否仍存在"的不确定性；新行（303 行）界定了"是否为外部接口"不属于本阶段判定范围。确认两行不矛盾——即：本阶段的不确定性只发生在"定义是否存在"这一维度上，不存在"因外部性存疑而淘汰"的适用场景。
3. **与 spec 的 tag 对照表一致。** 确认 §三 c/d 四条 reason 模板与 `docs/superpowers/specs/2026-09-22-api-fixer-reason-tags-design.md` 的 tag 对照表仍逐字一致（本次不改它们，仅确认未被误伤）。

如发现不一致，直接修复并重新核对。

- [ ] **Step 2: 回归实跑**

本缺陷非必现，实跑**不能**复现原始 bug，只能验证没有引入回归。按计划末尾「验收标准」执行。

- [ ] **Step 3: 最终提交**

```bash
git add api-fixer/SKILL.md
git commit -m "docs(api-fixer): final consistency pass for definition-judgment scope"
```

（spec `docs/superpowers/specs/2026-09-22-api-fixer-definition-criteria-design.md` 已在 brainstorming 阶段提交，本次不重复提交；若 Step 1 的核对过程需要修订 spec，才一并 `git add`。）

（若 Step 1 无改动，跳过本次提交，报告即可。）

---

## 验收标准（全部任务完成后）

对 `test_fixtures/project` 实跑一次 `/api-fixer`，确认未引入回归：

1. 备份现有 fixture：`cp test_fixtures/project/.ethunter_out/api-fixer/progress.json /tmp/api-fixer-progress.baseline3.json`
2. 删除 `test_fixtures/project/.ethunter_out/api-fixer/progress.json`（`phase = "done"` 会让 skill 直接停止，必须删除才能重新分析；`inherited_apis.json` 保留，运行会覆盖）
3. 派子代理按修改后的 `api-fixer/SKILL.md` 执行 `/api-fixer test_fixtures/project`。子代理须完整读取 SKILL.md 后按其流程处理 10 条 `old_api.json` 条目，且**不得读取**任何历史 progress.json（含 git 历史与 `/tmp`）、`test_fixtures/expected/`、`test_fixtures/TEST_PLAN.md`、`docs/superpowers/`
4. 先做逐字段比对（`git diff` 新旧 progress.json，检查每个字段的值），再确认 10 条判定与 fixture 完全一致：`[c1]`×5、`[d1]`×1、`[d2]`×1、`[d3]`×3，淘汰条目 `file` 为 `null`，继承条目 `file` 为绝对路径
5. 核对 `inherited_apis.json` 仍为 6 条，与 `test_fixtures/expected/api-fixer/inherited_apis.json` 一致
6. 比对通过后把实跑输出恢复到基线序列化格式（`git checkout -- <文件>`），避免留下纯格式噪音的 diff

**本次期望结果：与 fixture 零差异。** 本改动只是消除模型自行追加条件的空间，不改变任何正确判定。若出现差异，先判定是模型执行偏差还是文案缺陷——前者重跑（可提高子代理能力档位），后者回到 Task 2 Step 1 修复文案；**不得把差异直接改 fixture 迁就**。

**复现性说明：** 因本缺陷非必现，上述实跑通过**不构成**"bug 已修复"的证据；它只证明未引入回归。修复的直接依据是 Task 1 的文案层核对——即新增原则使"四条之外追加条件"在提示词中无立足点。
