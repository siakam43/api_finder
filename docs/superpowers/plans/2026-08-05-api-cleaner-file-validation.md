# api-cleaner 文件校验 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 api-cleaner 逐接口分析流程中新增 2.1.1 文件校验子步骤，确保每个接口的 file 字段有效且在分析范围内

**Architecture:** 在 `api-cleaner/SKILL.md` 的 2.1 读取源码和 2.2 快速排除检查之间插入一个新子步骤。校验失败直接排除接口，通过才进入后续分析

**Tech Stack:** Markdown

---

### Task 1: 在 2.1 和 2.2 之间插入 2.1.1 文件校验子步骤

**Files:**
- Modify: `api-cleaner/SKILL.md:183`

- [ ] **Step 1: 插入新子步骤**

定位到 `### 2.1 读取源码` 末尾（第 183 行空白行）和 `### 2.2 快速排除检查`（第 184 行）之间，插入以下内容：

```markdown

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
```

- [ ] **Step 2: 验证插入位置正确**

```bash
grep -n "### 2.1 读取源码\|### 2.1.1 文件校验\|### 2.2 快速排除检查" api-cleaner/SKILL.md
```

期望输出: 行号递增顺序为 2.1 → 2.1.1 → 2.2

- [ ] **Step 3: Commit**

```bash
git add api-cleaner/SKILL.md
git commit -m "feat(api-cleaner): add file validation pre-check before deep analysis

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```
