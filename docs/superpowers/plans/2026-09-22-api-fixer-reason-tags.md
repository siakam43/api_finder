# api-fixer reason tag 化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用 `[c1]` / `[d1]` / `[d2]` / `[d3]` 四种 tag 替代 api-fixer Step 2 中模糊的 `[fallback]` 前缀与 `null`，让 `progress.json` 的 `reason` 字段直接标明条目在 c/d 的哪个分支被判定。

**Architecture:** 纯文档修改，主文件 `api-fixer/SKILL.md`。仅改 reason 文案取值与字段说明，不改任何判定逻辑、分支条件、去重规则或输出结构。同步更新被 git 跟踪的 fixture 基线（`test_fixtures/project/.ethunter_out/api-fixer/progress.json`）与测试计划（`test_fixtures/TEST_PLAN.md`）。历史设计文档 `2026-08-07-api-fixer-design.md` 保持原样。

**Tech Stack:** Markdown、JSON

**Spec:** `docs/superpowers/specs/2026-09-22-api-fixer-reason-tags-design.md`

**验证方式：** 每个任务编辑后用 Read 重读被改小节，逐项核对下方"核对点"；fixture 改用 Python 脚本生成并用断言校验；最后派子代理实跑一次 `/api-fixer` 做端到端验证（见验收标准）。

---

### Task 1: progress.json 结构示例与字段说明 + Step 2c（[c1]）

**Files:**
- Modify: `api-fixer/SKILL.md:141`（§二 progress.json 结构示例）
- Modify: `api-fixer/SKILL.md:153`（§二 progress.json 结构，字段说明）
- Modify: `api-fixer/SKILL.md:220-222`（§三 流程 Step 2c 记录块）

- [ ] **Step 1: 更新结构示例的 reason 取值**

将（注意缩进为 5 空格）：

```
     "reason": null|"<处理原因>"},
```

替换为：

```
     "reason": "[tag] 判定依据"},
```

- [ ] **Step 2: 更新 reason 字段说明**

将：

```markdown
- `reason`：淘汰原因（`result = "inherited"` 时为 `null`；`result = "eliminated"` 时为具体淘汰理由）
```

替换为：

```markdown
- `reason`：判定依据，格式为 `"[tag] 说明"`（tag 取值见第三节 c、d 分支），所有条目均非 null
```

- [ ] **Step 3: 更新 Step 2c 的存在分支 reason**

将（注意缩进为 5 空格起始，逐层递增 3 空格）：

```
     ├── 函数定义存在 → 继承。记录：
     │     result = "inherited", path_updated = false,
     │     reason = null
```

替换为：

```
     ├── 函数定义存在 → 继承。记录：
     │     result = "inherited", path_updated = false,
     │     reason = "[c1] 原路径在分析范围内，且函数定义存在"
```

- [ ] **Step 4: 验证并提交**

用 Read 重读 `api-fixer/SKILL.md` 的第 138-145 行、147-155 行与 219-224 行。核对点：

- 结构示例的 `reason` 不再出现 `null`，改为 `"[tag] 判定依据"`；示例其余字段（`name`/`original_file`/`file`/`result`/`path_updated`/`phase`/`total`/`processed`）未变
- 字段说明不再出现"淘汰原因"，不再声称 inherited 时为 null
- 字段说明指向"第三节 c、d 分支"作为 tag 取值来源
- Step 2c 存在分支的 `result` 仍为 `"inherited"`、`path_updated` 仍为 `false`
- 全文不再出现 `null` 取值：`grep -n 'null' api-fixer/SKILL.md` 仅剩 1 处，即 L153 字段说明末尾的措辞"所有条目均非 null"（该措辞本身含 null 二字，属预期，不计为残留）；`grep -c 'reason = null' api-fixer/SKILL.md` 应为 0

```bash
git add api-fixer/SKILL.md
git commit -m "docs(api-fixer): tag c1 for in-scope inherit and redefine reason field"
```

---

### Task 2: Step 2d 三个分支（[d1]/[d2]/[d3]）

**Files:**
- Modify: `api-fixer/SKILL.md:225-239`（§三 流程 Step 2d）

- [ ] **Step 1: 替换 d1 分支的 reason**

将：

```
     ├── 已有同名继承 → 淘汰。记录：
     │     result = "eliminated", path_updated = false,
     │     reason = "[fallback] 函数定义未在原路径对应的文件中找到，且同名函数 <name> 已被其他 old_api 条目继承（文件：<已有条目file>），跳过 fallback"
```

替换为：

```
     ├── 已有同名继承 → 淘汰。记录：
     │     result = "eliminated", path_updated = false,
     │     reason = "[d1] 同名函数 <name> 已被其他 old_api 条目继承（文件：<已有条目file>），跳过 fallback"
```

- [ ] **Step 2: 替换 d2 分支的 reason**

将：

```
           ├── 找到一个或多个 → 取第一个匹配的文件。记录：
           │     path_updated = true, file = 新找到的绝对路径,
           │     reason = "原路径对应的文件中未找到函数定义，在 scope_files 中找到同名函数定义于 <新路径>"
```

替换为：

```
           ├── 找到一个或多个 → 取第一个匹配的文件。记录：
           │     path_updated = true, file = 新找到的绝对路径,
           │     reason = "[d2] 在 scope_files 中找到同名函数定义于 <新路径>"
```

- [ ] **Step 3: 替换 d3 分支的 reason**

将：

```
           └── 未找到 → 淘汰。记录：
                 result = "eliminated", path_updated = false,
                 reason = "[fallback] 函数定义未在原路径对应的文件中找到，且 scope_files 中未找到同名函数定义"
```

替换为：

```
           └── 未找到 → 淘汰。记录：
                 result = "eliminated", path_updated = false,
                 reason = "[d3] scope_files 中未找到同名函数定义"
```

- [ ] **Step 4: 验证并提交**

用 Read 重读 `api-fixer/SKILL.md` 的 225-242 行。核对点：

- `grep -c '\[fallback\]' api-fixer/SKILL.md` 输出 0（原来为 2）
- d1/d2/d3 三个记录块分别含 `[d1]`、`[d2]`、`[d3]`，且各出现 1 次
- d 的任何 reason 文案不再出现"函数定义未在原路径对应的文件中找到"或"原路径对应的文件中未找到函数定义"
- d1 保留"跳过 fallback"表述；d2/d3 的 `result` 与 `path_updated` 取值与修改前一致（d2 为 inherited/true，d3 为 eliminated/false）
- 分支标签"已有同名继承 → 淘汰""找到一个或多个""未找到 → 淘汰"均未被改动

```bash
git add api-fixer/SKILL.md
git commit -m "docs(api-fixer): replace fallback tag with d1/d2/d3 per-case tags"
```

---

### Task 3: §七 使用示例（首次分析 + 断点续分析）

**Files:**
- Modify: `api-fixer/SKILL.md:317-322`（§七 首次分析逐条输出）
- Modify: `api-fixer/SKILL.md:340,342`（§七 断点续分析逐条输出）

- [ ] **Step 1: 更新首次分析示例**

将：

```
    → 条目 1/15: func_a — 继承（路径在范围且函数定义存在，路径未变）
    → 条目 2/15: func_b — 继承（路径不在范围，fallback 搜索到新路径，路径已更新）
    → 条目 3/15: func_c — 淘汰（函数定义未找到，scope_files 中也无同名函数）
    → 条目 4/15: func_d — 淘汰（同名函数已被条目 2 继承，跳过 fallback）
    → ...
    → 条目 15/15: func_o — 继承（路径在范围且函数定义存在）
```

替换为：

```
    → 条目 1/15: func_a — 继承 [c1]
    → 条目 2/15: func_b — 继承 [d2]（fallback 搜索到新路径，路径已更新）
    → 条目 3/15: func_c — 淘汰 [d3]（scope_files 中未找到同名函数定义）
    → 条目 4/15: func_d — 淘汰 [d1]（同名函数已被条目 2 继承，跳过 fallback）
    → ...
    → 条目 15/15: func_o — 继承 [c1]
```

- [ ] **Step 2: 更新断点续分析示例**

将：

```
    → 条目 8/15: func_h — 继承
```

替换为：

```
    → 条目 8/15: func_h — 继承 [c1]
```

将（§七 断点续分析末尾那条，全文仅此一处，替换时用下方更长上下文定位）：

```
    → 条目 15/15: func_o — 继承

  分析完成。继承: 12 | 淘汰: 3
```

替换为：

```
    → 条目 15/15: func_o — 继承 [c1]

  分析完成。继承: 12 | 淘汰: 3
```

- [ ] **Step 3: 验证并提交**

用 Read 重读 `api-fixer/SKILL.md` 的 308-346 行。核对点：

- 首次分析 5 条输出（1、2、3、4、15）全部带 tag，且与正文分支一一对应：1→`[c1]`、2→`[d2]`、3→`[d3]`、4→`[d1]`、15→`[c1]`
- 断点续分析 2 条输出（8、15）均带 `[c1]`
- 两处 `分析完成。继承: 12 | 淘汰: 3` 与 `总计: 15 个历史接口 | 继承: 12 | 淘汰: 3` 汇总行未被改动
- 示例中文案与 §三 正文 reason 取值不矛盾（示例展示的是 tag，不复制完整 reason）

```bash
git add api-fixer/SKILL.md
git commit -m "docs(api-fixer): show reason tags in usage examples"
```

---

### Task 4: fixture progress.json 重写

**Files:**
- Modify: `test_fixtures/project/.ethunter_out/api-fixer/progress.json`

- [ ] **Step 1: 用脚本重写 10 条 reason**

在仓库根目录运行（脚本以原文件格式逐条单行输出，避免 `json.dump(indent=2)` 把每条撑成多行）：

```bash
python3 - <<'PY'
import json

P = "/home/admin/cc/wksp/siakam_security_skills/api_finder/test_fixtures/project"
path = P + "/.ethunter_out/api-fixer/progress.json"

C1 = "[c1] 原路径在分析范围内，且函数定义存在"
D1 = "[d1] 同名函数 duplicate_name 已被其他 old_api 条目继承（文件：" + P + "/core/dispatcher.c），跳过 fallback"
D2 = "[d2] 在 scope_files 中找到同名函数定义于 " + P + "/comm/msg_handler.c"
D3 = "[d3] scope_files 中未找到同名函数定义"

rows = [
    ("handle_user_request", P + "/core/dispatcher.c", P + "/core/dispatcher.c", "inherited", False, C1),
    ("handler_func_b", P + "/core/dispatcher.c", P + "/comm/msg_handler.c", "inherited", True, D2),
    ("handler_func_c", P + "/core/dispatcher.c", P + "/core/dispatcher.c", "eliminated", False, D3),
    ("legacy_handler", P + "/core/legacy.c", P + "/core/legacy.c", "eliminated", False, D3),
    ("duplicate_name", P + "/core/dispatcher.c", P + "/core/dispatcher.c", "inherited", False, C1),
    ("duplicate_name", P + "/comm/msg_handler.c", P + "/comm/msg_handler.c", "eliminated", False, D1),
    ("process_ipc_message", P + "/comm/msg_handler.c", P + "/comm/msg_handler.c", "inherited", False, C1),
    ("read_from_shared_memory", "comm/msg_handler.c", P + "/comm/msg_handler.c", "inherited", False, C1),
    ("handle_ipc_queue", "io/ipc_handler.c", P + "/io/ipc_handler.c", "inherited", False, C1),
    ("deleted_api", P + "/core/removed.c", P + "/core/removed.c", "eliminated", False, D3),
]

entries = [
    {"name": n, "original_file": o, "file": f, "result": r, "path_updated": u, "reason": w}
    for n, o, f, r, u, w in rows
]

lines = ['{', '  "phase": "done",', '  "total": 10,', '  "processed": 10,', '  "results": [']
for i, e in enumerate(entries):
    sep = "," if i < len(entries) - 1 else ""
    lines.append("    " + json.dumps(e, ensure_ascii=False) + sep)
lines += ['  ]', '}']

with open(path, "w") as fh:
    fh.write("\n".join(lines) + "\n")
PY
```

- [ ] **Step 2: 断言校验**

```bash
python3 -c "
import json
p = 'test_fixtures/project/.ethunter_out/api-fixer/progress.json'
d = json.load(open(p))
r = d['results']
assert d['phase'] == 'done' and d['total'] == 10 and d['processed'] == 10, d['phase']
assert len(r) == 10, len(r)
assert all(e['reason'] for e in r), '仍有 null reason'
tags = [e['reason'][:4] for e in r]
assert tags.count('[c1]') == 5, tags
assert tags.count('[d1]') == 1, tags
assert tags.count('[d2]') == 1, tags
assert tags.count('[d3]') == 3, tags
assert r[0]['result'] == 'inherited' and r[0]['path_updated'] is False
assert r[1]['result'] == 'inherited' and r[1]['path_updated'] is True and r[1]['file'].endswith('/comm/msg_handler.c')
assert r[5]['result'] == 'eliminated' and r[5]['reason'].startswith('[d1] 同名函数 duplicate_name')
assert r[7]['original_file'] == 'comm/msg_handler.c', r[7]['original_file']
assert r[8]['original_file'] == 'io/ipc_handler.c', r[8]['original_file']
assert '[fallback]' not in open(p).read()
print('fixture assertions passed')
"
```

预期输出：`fixture assertions passed`

- [ ] **Step 3: 核对 diff 只动了 reason**

```bash
git diff -U0 test_fixtures/project/.ethunter_out/api-fixer/progress.json | grep '^[+-]' | grep -v '^[+-][+-]'
```

核对点：加号行与减号行数量相等（10 增 10 删），且每对的差异只在 `reason` 字段——`name` / `original_file` / `file` / `result` / `path_updated` 逐字未变；第 8、9 条的 `original_file` 仍为相对路径。

- [ ] **Step 4: Commit**

```bash
git add test_fixtures/project/.ethunter_out/api-fixer/progress.json
git commit -m "test(api-fixer): update progress fixture for per-case reason tags"
```

---

### Task 5: TEST_PLAN.md 预期 tag

**Files:**
- Modify: `test_fixtures/TEST_PLAN.md:16-26`（api-fixer 测试用例表）

- [ ] **Step 1: 在预期结果列补 tag**

将：

```markdown
| # | 场景 | old_api 条目 | 预期结果 |
|---|------|-------------|---------|
| 1 | 路径在范围，函数定义存在 | handle_user_request → core/dispatcher.c | inherited, path unchanged |
| 2 | 路径在范围，函数已迁移 | handler_func_b → core/dispatcher.c (实际在 comm/) | inherited, path_updated=true |
| 3 | 路径在范围，函数已删除 | handler_func_c → core/dispatcher.c (不存在) | eliminated |
| 4 | 路径不在范围（文件不存在），fallback 无结果 | legacy_handler → core/legacy.c | eliminated |
| 5 | 同名去重：第一个继承，第二个淘汰 | duplicate_name ×2 | 1 inherited + 1 eliminated |
| 6 | 绝对路径，正常继承 | process_ipc_message | inherited |
| 7 | 相对路径，正常化后继承 | read_from_shared_memory (相对路径) | inherited |
| 8 | 相对路径，正常化后继承 | handle_ipc_queue (相对路径) | inherited |
| 9 | 文件不存在，fallback 无结果 | deleted_api → core/removed.c | eliminated |
```

替换为：

```markdown
| # | 场景 | old_api 条目 | 预期结果 |
|---|------|-------------|---------|
| 1 | 路径在范围，函数定义存在 | handle_user_request → core/dispatcher.c | inherited, path unchanged, `[c1]` |
| 2 | 路径在范围，函数已迁移 | handler_func_b → core/dispatcher.c (实际在 comm/) | inherited, path_updated=true, `[d2]` |
| 3 | 路径在范围，函数已删除 | handler_func_c → core/dispatcher.c (不存在) | eliminated, `[d3]` |
| 4 | 路径不在范围（文件不存在），fallback 无结果 | legacy_handler → core/legacy.c | eliminated, `[d3]` |
| 5 | 同名去重：第一个继承，第二个淘汰 | duplicate_name ×2 | 1 inherited `[c1]` + 1 eliminated `[d1]` |
| 6 | 绝对路径，正常继承 | process_ipc_message | inherited, `[c1]` |
| 7 | 相对路径，正常化后继承 | read_from_shared_memory (相对路径) | inherited, `[c1]` |
| 8 | 相对路径，正常化后继承 | handle_ipc_queue (相对路径) | inherited, `[c1]` |
| 9 | 文件不存在，fallback 无结果 | deleted_api → core/removed.c | eliminated, `[d3]` |
```

- [ ] **Step 2: 验证并提交**

用 Read 重读 `test_fixtures/TEST_PLAN.md` 的 14-28 行。核对点：

- 9 行的预期结果列全部带 tag，取值与 fixture progress.json 的 10 条一致（用例 5 对应 2 条）
- 列分隔符与表头行对齐，加 tag 后每行仍为 4 列
- "**预期 api-fixer inherited_apis.json: 6 个条目**"未改动（该文件只含 name + file，不受 reason 变更影响）
- 场景列文案未改动

```bash
git add test_fixtures/TEST_PLAN.md
git commit -m "test(api-fixer): record expected reason tags in test plan"
```

---

### Task 6: 全文一致性核对与最终提交

**Files:**
- Review: `api-fixer/SKILL.md`（全文）

- [ ] **Step 1: 重读全文，逐项核对 Spec**

用 Read 通读整个 `api-fixer/SKILL.md`，按以下清单核对（对应 spec `docs/superpowers/specs/2026-09-22-api-fixer-reason-tags-design.md`）：

1. §二 字段说明：`reason` 定义为"判定依据，格式为 `"[tag] 说明"`（tag 取值见第三节 c、d 分支），所有条目均非 null"
2. §三 Step 2c：存在分支 reason = `"[c1] 原路径在分析范围内，且函数定义存在"`
3. §三 Step 2d：d1/d2/d3 三条 reason 与 spec 的 tag 对照表逐字一致
4. §七 首次分析：5 条输出带 tag，映射为 1→c1、2→d2、3→d3、4→d1、15→c1
5. §七 断点续分析：2 条输出带 `[c1]`
6. 全文无残留 `[fallback]`，无残留 `reason = null`
7. 全文无与本次修改矛盾的段落——§四 约束规则第 4 条"严格按照 a → b → c → d 顺序"、§六 抗理性化检查表（均只描述流程与步骤，不涉及 reason 取值，应无需改动）
8. **恢复路径与 L153 断言的矛盾：** §二 入口恢复流程会原样保留 `results` 中已处理的条目（L170-173"保留已处理结果，跳过"），而 §二 给出的唯一补救（L167-168）只针对 `phase = "done"`。若传入的是改动前写的 `progress.json`（`phase = "processing"`，含 `null` / `[fallback]` / 无 tag 的 d2 文案），恢复后文件会混合旧值与新 tag，与 L153 新增的"所有条目均非 null / 格式为 `"[tag] 说明"`"矛盾。spec 影响范围已声明"不考虑历史结果兼容"，但该决定未写进 SKILL.md。**决策点：** 是否在 §二 补一句"改动前生成的 progress.json 不做迁移，需删除后重新分析"（属新增范围，需用户确认后再改，不得擅自加）。
9. **用词一致性：** L141 用"判定依据"、L153 用"说明"指同一位置，违反 §四 规则 6"相同语义的用词前后保持一致"。统一为同一个词（建议"判定依据"），spec 中对应措辞一并同步。

如发现不一致，直接修复并重新核对。

- [ ] **Step 2: 核对待办项缺失**

```bash
grep -rn 'fallback\]' api-fixer/SKILL.md test_fixtures/ docs/superpowers/specs/2026-09-22-api-fixer-reason-tags-design.md
```

核对点：

- `api-fixer/SKILL.md` 与 `test_fixtures/` 下**无** `[fallback]` 命中
- spec 文件中的 `[fallback]` 命中（"问题"章节描述旧状态）属预期，保留
- `api-fixer/SKILL.md` 中 d1 的"跳过 fallback"与 `test_fixtures/TEST_PLAN.md` 场景列中的"fallback 无结果"不含方括号，不属命中，本次不改（fallback 仍是 d 步的步骤名）

- [ ] **Step 3: 最终提交**

```bash
git add api-fixer/SKILL.md test_fixtures/TEST_PLAN.md
git commit -m "docs(api-fixer): final consistency pass for reason tags"
```

（若 Step 1/2 无改动，跳过本次提交，报告即可。）

---

## 验收标准（全部任务完成后）

对 `test_fixtures/project` 实跑一次 `/api-fixer`，检查新生成的 `progress.json`：

1. 备份现有 fixture：`cp test_fixtures/project/.ethunter_out/api-fixer/progress.json /tmp/api-fixer-progress.baseline.json`
2. 删除 `test_fixtures/project/.ethunter_out/api-fixer/progress.json`（`phase = "done"` 会让 skill 直接停止，必须删除才能重新分析；`inherited_apis.json` 保留，运行会覆盖）
3. 派子代理按修改后的 `api-fixer/SKILL.md` 执行 `/api-fixer test_fixtures/project`（子代理需完整读取 SKILL.md 后按其流程处理 10 条 `old_api.json` 条目）
4. 核对新输出的 10 条 `reason`：tag 序列与 fixture 逐条一致（`[c1]`×5、`[d1]`×1、`[d2]`×1、`[d3]`×3），且每条 tag 对应的分支与 TEST_PLAN.md 的场景描述相符
5. 核对 `inherited_apis.json` 仍为 6 条，与 `test_fixtures/expected/api-fixer/inherited_apis.json` 一致
6. 若新输出与 fixture 有合理差异（如 d2 命中的文件不同），保留实测输出作为新 fixture 并提交；若完全一致则无需再提交

端到端跑通仅验证两条真实入口路径（b 入口：`legacy_handler`、`deleted_api`；c 入口：`handler_func_c`），四种 tag 均被覆盖。
