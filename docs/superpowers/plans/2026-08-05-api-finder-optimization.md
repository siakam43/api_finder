# api-finder 优化 — file 字段修正与结果校验 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 api-finder SKILL.md 中 file 字段语义不明确的问题，并新增分析范围校验子步骤过滤无效结果

**Architecture:** 对 `api-finder/SKILL.md` 做三处独立编辑：第五步、第六步各补充一句 file 字段说明，第七步在审查循环和输出之间插入新的"分析范围校验"子步骤

**Tech Stack:** Markdown

---

### Task 1: 第五步 feature/match — 补充 file 字段说明

**Files:**
- Modify: `api-finder/SKILL.md:428-431`

- [ ] **Step 1: 在 candidate_apis 条目格式后追加说明**

定位到第五步"子阶段二：推广匹配（match）"中 candidate_apis 条目格式定义处（第 428-431 行），在 ` ``` ` 代码块结束后追加以下说明：

```markdown

	**重要：** `file` 字段必须填写**函数定义**所在的文件绝对路径。通过注册数组/结构体定义或注册函数调用点找到候选函数后，必须以函数名为关键词在 scope_files 中搜索函数定义位置（优先 codegraph_explore，否则 grep 搜索函数体定义 `函数名(` 后跟 `{` 的模式），将定义所在文件路径填入 `file`，而非注册点/注册数组所在文件路径。
```

- [ ] **Step 2: 验证编辑位置正确**

```bash
grep -n "candidate_apis 条目格式" api-finder/SKILL.md
```

- [ ] **Step 3: Commit**

```bash
git add api-finder/SKILL.md
git commit -m "docs(api-finder): clarify file field stores function definition in feature/match step

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: 第六步 arch_identify — 补充 file 字段说明

**Files:**
- Modify: `api-finder/SKILL.md:563-567`

- [ ] **Step 1: 在 found_apis 记录格式说明后追加说明**

定位到第六步"架构识别接口"中 found_apis 记录格式定义处（第 563-567 行），在"注意"行之前插入以下说明：

```markdown

	**重要：** `file` 字段必须填写**函数定义**所在的文件绝对路径。当前在函数定义处进行分析，`file` 填写当前分析文件路径即可。

```

- [ ] **Step 2: 验证编辑位置正确**

```bash
grep -n "记录发现到 found_apis" api-finder/SKILL.md
```

- [ ] **Step 3: Commit**

```bash
git add api-finder/SKILL.md
git commit -m "docs(api-finder): clarify file field stores function definition in arch_identify step

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: filter_state.json 结构 — 新增 validated 字段

**Files:**
- Modify: `api-finder/SKILL.md:636-638`

- [ ] **Step 1: 在 filter_state.json 结构定义的 api_list 条目中增加 validated 字段**

定位到第七步"进入检查"中 filter_state.json 结构定义处（第 628-638 行），将 api_list 条目结构从：

```json
{"name": "<函数名>", "file": "<文件路径>", "reviewed": false, "decision": null, "reason": null}
```

改为：

```json
{"name": "<函数名>", "file": "<文件路径>", "reviewed": false, "decision": null, "reason": null, "validated": false}
```

同时将 desc 文本中"清空 decision/reason"改为"清空 decision/reason，validated 设为 false"。

- [ ] **Step 2: Commit**

```bash
git add api-finder/SKILL.md
git commit -m "docs(api-finder): add validated field to filter_state.json schema

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 4: 第七步 filter — 新增"分析范围校验"子步骤

**Files:**
- Modify: `api-finder/SKILL.md:716` (在第 716 行"7. 继续下一条..."和第 717 行"### 输出结果"之间插入)

- [ ] **Step 1: 插入分析范围校验子步骤**

在逐条审查循环的步骤 7（第 716 行"7. 继续下一条 `reviewed = false` 的条目，直到全部审查完毕。"）之后、"### 输出结果"之前，插入以下完整子步骤：

```markdown

### 分析范围校验

逐条审查循环完成后，对 api_list 中所有 `decision = "keep"` 且 `validated = false` 的条目执行以下校验：

1. **函数定义验证：** 在 `file` 指向的文件中用 grep 搜索函数定义（匹配模式：函数名后跟 `(` 且最终包含 `{` 的函数体定义）。
   - 包含函数定义 → 通过，`file` 不变
   - 不包含函数定义 → 在全部 scope_files 中搜索该函数定义
     - 找到 → 更新 `file` 为正确的定义文件绝对路径
     - 找不到 → `decision` 改为 `"exclude"`，`reason` 记录 `"函数定义未找到"`

2. **分析范围验证：** 校验 `file` 是否在 scope_files 列表中。
   - 在 scope_files 中 → 通过
   - 不在 scope_files 中 → `decision` 改为 `"exclude"`，`reason` 记录 `"接口不在代码分析范围内"`

3. 将 `validated` 设为 `true`。
4. **每校验完一条就保存一次 filter_state.json**，防止中断丢失进度。
5. 继续下一条 `validated = false` 的条目，直到全部校验完毕。

**断点恢复时**，跳过 `validated = true` 的条目，从第一个 `validated = false` 的条目继续校验。
```

- [ ] **Step 2: 更新"输出结果"小节**

将"输出结果"小节的第一行从：
```markdown
从 api_list 中提取所有 `decision = "keep"` 的条目，按原始顺序写入：
```

改为：
```markdown
从 api_list 中提取所有 `decision = "keep"` 且 `validated = true` 的条目，按原始顺序写入：
```

- [ ] **Step 3: 验证修改后的结构完整性**

```bash
grep -n "### 输出结果\|### 分析范围校验\|### 分析完成" api-finder/SKILL.md
```

确保子步骤顺序为：逐条审查循环 → 分析范围校验 → 输出结果 → 分析完成

- [ ] **Step 4: Commit**

```bash
git add api-finder/SKILL.md
git commit -m "feat(api-finder): add scope validation sub-step in filter phase

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```
