# api-finder: old_api 淘汰接口追踪 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Track every old_api.json interface's fate (inherited or eliminated with reason) across Section 4 and Section 7, and append an elimination summary to finder_summary.md.

**Architecture:** Modify the api-finder SKILL.md (single file) to add: (1) old_api_fate.json creation in Section 4, (2) origin tracking in Section 7 filter_state.json, (3) write-back of Section 7 eliminations to old_api_fate.json, and (4) appendix output in finder_summary.md. No new files are created — all changes are edits to the existing SKILL.md.

**Tech Stack:** Markdown skill definition file. No code — this is a prompt/skill documentation change.

---

### Task 1: Section 4 — Add old_api_fate.json creation

**Files:**
- Modify: `api-finder/SKILL.md:227-264`

Replace Section 4's "流程" block to add old_api_fate.json tracking alongside the existing inherited_apis.json output.

- [ ] **Step 1: Replace Section 4 flow block**

Replace the existing flow block (lines 233-261) with the new version that creates old_api_fate.json:

```
1. 用两种方法检查 <project_dir>/.ethunter_out/api-finder/conf/old_api.json 是否存在。
   根据判断规则：
   ├── 确认不存在 → 写入空的 inherited_apis.json（内容为 []），
   │     更新 progress.json 中 inherit.status = "completed"，phase = "feature"，继续下一阶段。
   │     （不创建 old_api_fate.json）
   └── 确认存在 → 继续

2. 读取 old_api.json。其格式为：
   [
     {"name": "FUNC_NAME", "file": "FILE_PATH"},
     ...
   ]
   逐一检查每个条目，分两步验证，为每个条目记录去向：

   Step 2a — 文件路径检查：该接口的 file 字段是否在 scope_files 中存在？
   ├── 不在 scope_files 中 → 淘汰。记录：fate = "eliminated", stage = "inherit",
   │     reason = "Step 2a：文件 <file> 不在本次 scope_files 分析范围内"
   └── 在 scope_files 中 → 继续 Step 2b

   Step 2b — 函数存在性检查：在该文件中搜索函数定义是否依然存在。
      使用 `grep -n "<函数名>" <文件路径>` 搜索，确认文件中存在该函数的定义
      （不仅匹配声明/引用，需要确认有函数体定义。例如匹配 `函数名(` 后跟 `{` 的模式）。
      ├── 函数定义不存在 → 淘汰。记录：fate = "eliminated", stage = "inherit",
      │     reason = "Step 2b：在文件 <file> 中未找到函数 <name> 的定义（可能已删除、重命名或移至其他文件）"
      └── 函数定义存在 → 继承。记录：fate = "inherited", stage = null, reason = null

3. 写入两份文件：
   - 将 fate = "inherited" 的条目写入 <project_dir>/.ethunter_out/api-finder/tmp/inherited_apis.json，
     格式与 old_api.json 一致（仅 name + file，供第五节和后续使用）。
   - 将全部条目（含 inherited 和 eliminated）写入
     <project_dir>/.ethunter_out/api-finder/tmp/old_api_fate.json，格式为：
     [
       {"name": "<函数名>", "file": "<文件绝对路径>", "fate": "inherited|eliminated", "stage": null|"inherit", "reason": null|"<淘汰原因>"},
       ...
     ]
     其中 file 保持 old_api.json 中的原始值。

4. 更新 progress.json：inherit.status = "completed"，phase = "feature"。
```

- [ ] **Step 2: Verify the edit**

Read `api-finder/SKILL.md:227-264` to confirm the new content replaced the old, and the rest of the file is intact.

---

### Task 2: Section 7 — Add origin field to merge and filter_state.json

**Files:**
- Modify: `api-finder/SKILL.md:599-609` (合并接口列表)
- Modify: `api-finder/SKILL.md:630-640` (filter_state.json 结构)

- [ ] **Step 1: Update merge step to include origin tagging**

Replace the merge step (lines 601-609) to add origin assignment:

```
1. 读取以下文件（如果某文件为空数组或不存在，跳过该文件）：
   - tmp/inherited_apis.json → 这些条目的 origin 为 "inherit"
   - tmp/feature_apis.json → 这些条目的 origin 为 "feature"
   - tmp/arch_apis.json → 这些条目的 origin 为 "arch_identify"

2. 合并所有条目，按 name + file 去重（两个条目函数名和文件路径都一致视为重复，保留一个即可）。
   去重时，如果多个来源包含同一接口，优先保留 origin = "inherit"（历史接口优先），
   其余 origin 值丢弃即可。
   去重后的列表称为 api_pool。每个条目为：
   {"name": "<函数名>", "file": "<文件路径>", "origin": "inherit|feature|arch_identify"}
```

- [ ] **Step 2: Update filter_state.json schema to include origin**

Replace the filter_state.json schema (lines 632-639):

```json
{
  "api_total": <api_pool条目数>,
  "api_list": [
    {"name": "<函数名>", "file": "<文件路径>", "origin": "inherit|feature|arch_identify", "reviewed": false, "decision": null, "reason": null, "validated": false},
    ...
  ]
}
```

And update the description below it (line 642):

```
全新启动时，将 api_pool 中全部条目写入 api_list（保留 origin 字段），清空 decision/reason，validated 设为 false。
```

- [ ] **Step 3: Verify the edits**

Read the modified sections to confirm correctness.

---

### Task 3: Section 7 — Add write-back step after validation

**Files:**
- Modify: `api-finder/SKILL.md:739` (after 分析范围校验, before 输出结果)

- [ ] **Step 1: Insert write-back step between validation and output**

After the validation section (after line 739, before `### 输出结果` on line 741), insert:

```
### 回写 old_api 淘汰记录

逐条审查和分析范围校验全部完成后，将 old_api 继承接口的淘汰结果回写到 old_api_fate.json：

```
1. 用两种方法检查 <project_dir>/.ethunter_out/api-finder/tmp/old_api_fate.json 是否存在。
   根据判断规则：
   ├── 确认不存在 → 跳过回写（old_api.json 本身不存在，无历史接口可追踪）
   └── 确认存在 → 继续

2. 读取 old_api_fate.json。

3. 遍历 api_list 中所有条目：
   对每个 origin = "inherit" 且 decision = "exclude" 的条目：
   - 在 old_api_fate.json 中按 name 匹配对应条目
   - 将该条目的 fate 从 "inherited" 改为 "eliminated"
   - 将 stage 设为 "filter"
   - 将 reason 改为 entry.reason（即第七节审查/校验阶段记录的确凿证据）
   - 将 file 更新为 entry.file（校验阶段可能已更新文件路径）
   - 保存 old_api_fate.json

4. 完成回写（已继承保留的条目 fate = "inherited" 不变）。
```

匹配用 `name` 而非 `name + file`，因为校验阶段可能更新 `file` 字段（函数在另一文件中找到定义）。old_api.json 中函数名唯一，`name` 匹配已足够。
```

- [ ] **Step 2: Verify the edit**

Read the area around lines 739-741 to confirm the new section is correctly placed.

---

### Task 4: Output — Add old_api elimination appendix to finder_summary.md

**Files:**
- Modify: `api-finder/SKILL.md:755-772` (输出结果 finder_summary.md section)

- [ ] **Step 1: Add old_api appendix generation step**

After the finder_summary.md output block (after line 768, before the "更新 progress.json" step), insert step 2.5:

```
2.5. **old_api 接口去向附节** — 在 finder_summary.md 末尾追加：

用两种方法检查 <project_dir>/.ethunter_out/api-finder/tmp/old_api_fate.json 是否存在。
├── 确认存在 → 读取 old_api_fate.json，生成附节内容并追加到 finder_summary.md 末尾
└── 确认不存在 → 跳过附节（无 old_api 历史接口）

附节内容格式：

```markdown
---

## 附：old_api 接口去向说明

old_api.json 共 **N** 个历史接口：
- 继承保留：**X** 个
- 淘汰：**Y** 个

### 淘汰接口详述

#### 第四节（接口继承阶段）淘汰 — Z 个

**<序号>. <函数名> — <原文件路径>**
- **淘汰阶段：** 接口继承
- **淘汰原因：** Step 2a：文件 /path/to/old/file.c 不在本次 scope_files 分析范围内

#### 第七节（外部接口筛选阶段）淘汰 — W 个

**<序号>. <函数名> — <文件路径>**
- **淘汰阶段：** 外部接口筛选
- **淘汰原因：** 规则二：无外部输入 — 函数的 data 参数经追踪为内核内部缓冲区，不来自外部不可信源
```

统计来源：
- N = old_api_fate.json 条目总数
- X = fate = "inherited" 的条目数
- Y = fate = "eliminated" 的条目数
- Z = fate = "eliminated" 且 stage = "inherit" 的条目数
- W = fate = "eliminated" 且 stage = "filter" 的条目数

约束：Z + W = Y，X + Y = N。

fate = "inherited" 的接口已在 finder_summary.md 正文中逐条详述，附节不重复。
若 Z = 0 或 W = 0，对应小节省略不输出。
若 Y = 0（全部继承），省去"淘汰接口详述"小节，仅保留开头统计。
```

- [ ] **Step 3: Renumber the existing step 3**

The original step 3 (`更新 progress.json`) now becomes step 3 (unchanged content).

- [ ] **Step 4: Verify the edit**

Read the full output section to confirm correct structure.

---

### Task 5: Final review — Cross-check against spec and original flow

**Files:**
- Read: `api-finder/SKILL.md` (full file)

- [ ] **Step 1: Verify no original flow is broken**

Check each of these invariants against the modified SKILL.md:
- Section 5 still reads inherited_apis.json, which has the same format as before ✅
- Section 6 (arch_identify) is untouched ✅
- Section 7 review/validation logic is unchanged (only additive changes) ✅
- api.json output format is unchanged ✅
- finder_summary.md main body format is unchanged ✅
- progress.json structure is unchanged (no new phase added) ✅
- All Bash/Read/Write permissions already covered in Section 8 ✅

- [ ] **Step 2: Verify spec coverage**

Cross-check each spec requirement:
- [ ] old_api_fate.json created in Section 4 with all entries
- [ ] old_api.json not existing → skip file creation
- [ ] filter_state.json gets origin field
- [ ] Merge step assigns origin based on source file
- [ ] Write-back after validation updates old_api_fate.json
- [ ] Match by name, not name+file
- [ ] File field synced during write-back
- [ ] finder_summary.md appendix with full accounting
- [ ] Statistical constraints (Z+W=Y, X+Y=N)
- [ ] Empty sections handled (Z=0, W=0, Y=0)

- [ ] **Step 5: Commit**

```bash
git add api-finder/SKILL.md
git commit -m "feat(api-finder): track old_api elimination across inherit and filter stages

Add old_api_fate.json to record every old_api interface's fate (inherited
or eliminated with reason). Append elimination summary to finder_summary.md
for full accountability of historical API interfaces.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```
