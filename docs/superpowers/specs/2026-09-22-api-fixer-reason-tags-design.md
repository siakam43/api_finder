# api-fixer reason tag 化设计

## 问题

api-fixer §三 流程 Step 2 的 c、d 两步在 `progress.json` 的 `reason` 字段上存在三个问题：

1. **d 步的不同情况无法区分。** d 有三种情况（同名已继承淘汰、fallback 搜到转继承、fallback 未找到淘汰），其中两种淘汰情况统一加 `[fallback]` 前缀，看到 `[fallback]` 无法判断是第几种情况。
2. **c 步继承时 `reason = null`。** 看不到判定路径，无法从 `progress.json` 区分"路径在范围内且函数定义存在"与其他继承来源。
3. **d 的淘汰文案与实际执行不符。** 现有文案写"函数定义未在原路径对应的文件中找到"，但 d 有两个入口：从 b 进入时（路径不在 scope_files）**从未做过函数存在性检查**；只有从 c 进入时才做过。fixture 中的 `legacy_handler → core/legacy.c`（文件不存在、从 b 直接进 d）就是文案失真的实例。

## 方案

用"每情况一种 tag"替代 `[fallback]`，并统一 `reason` 语义。

### tag 对照

| tag | 触发条件 | result | path_updated | reason |
|-----|---------|--------|--------------|--------|
| `[c1]` | 路径在 scope_files 中，且函数定义存在 | `inherited` | `false` | `[c1] 原路径在分析范围内，且函数定义存在` |
| `[d1]` | 同名函数已被其他 old_api 条目继承 | `eliminated` | `false` | `[d1] 同名函数 <name> 已被其他 old_api 条目继承（文件：<已有条目file>），跳过 fallback` |
| `[d2]` | 无同名继承，scope_files 中找到同名函数定义 | `inherited` | `true` | `[d2] 在 scope_files 中找到同名函数定义于 <新路径>` |
| `[d3]` | 无同名继承，scope_files 中未找到同名函数定义 | `eliminated` | `false` | `[d3] scope_files 中未找到同名函数定义` |

- tag 编号按 §三 流程文档中 c、d 各分支的出现顺序。
- `[fallback]` 前缀全部删除。
- d 的文案不再声称做过函数存在性检查——能到达 d 即说明原路径有问题（b 未通过，或 b 通过但 c 未通过），执行者无需复述检查过程。
- `[d1]` 保留"跳过 fallback"表述，说明为何未执行搜索。
- `<name>`、`<已有条目file>`、`<新路径>` 为运行期填入的动态值。

### 字段语义变更

`<project_dir>/.ethunter_out/api-fixer/progress.json` 的 `reason` 字段：

- 旧：`reason`：淘汰原因（`result = "inherited"` 时为 `null`；`result = "eliminated"` 时为具体淘汰理由）
- 新：`reason`：判定依据，格式为 `"[tag] 说明"`（tag 取值见 c、d 分支），**所有条目均非 null**

`results` 条目其余字段（`name` / `original_file` / `file` / `result` / `path_updated`）不变。

## 变更清单

### SKILL.md §二 progress.json 结构（reason 字段说明）

- 旧：``- `reason`：淘汰原因（`result = "inherited"` 时为 `null`；`result = "eliminated"` 时为具体淘汰理由）``
- 新：``- `reason`：判定依据，格式为 `"[tag] 说明"`（tag 取值见第三节 c、d 分支），所有条目均非 null``

### SKILL.md §三 流程 Step 2c

- 旧：`reason = null`
- 新：`reason = "[c1] 原路径在分析范围内，且函数定义存在"`

### SKILL.md §三 流程 Step 2d — 已有同名继承

- 旧：`reason = "[fallback] 函数定义未在原路径对应的文件中找到，且同名函数 <name> 已被其他 old_api 条目继承（文件：<已有条目file>），跳过 fallback"`
- 新：`reason = "[d1] 同名函数 <name> 已被其他 old_api 条目继承（文件：<已有条目file>），跳过 fallback"`

### SKILL.md §三 流程 Step 2d — 找到同名函数定义

- 旧：`reason = "原路径对应的文件中未找到函数定义，在 scope_files 中找到同名函数定义于 <新路径>"`
- 新：`reason = "[d2] 在 scope_files 中找到同名函数定义于 <新路径>"`

### SKILL.md §三 流程 Step 2d — 未找到

- 旧：`reason = "[fallback] 函数定义未在原路径对应的文件中找到，且 scope_files 中未找到同名函数定义"`
- 新：`reason = "[d3] scope_files 中未找到同名函数定义"`

### SKILL.md §七 使用示例 — 首次分析

逐条输出改为带 tag（示例与正文一致）：

- 旧：`→ 条目 1/15: func_a — 继承（路径在范围且函数定义存在，路径未变）`
- 新：`→ 条目 1/15: func_a — 继承 [c1]`

- 旧：`→ 条目 2/15: func_b — 继承（路径不在范围，fallback 搜索到新路径，路径已更新）`
- 新：`→ 条目 2/15: func_b — 继承 [d2]（fallback 搜索到新路径，路径已更新）`

- 旧：`→ 条目 3/15: func_c — 淘汰（函数定义未找到，scope_files 中也无同名函数）`
- 新：`→ 条目 3/15: func_c — 淘汰 [d3]（scope_files 中未找到同名函数定义）`

- 旧：`→ 条目 4/15: func_d — 淘汰（同名函数已被条目 2 继承，跳过 fallback）`
- 新：`→ 条目 4/15: func_d — 淘汰 [d1]（同名函数已被条目 2 继承，跳过 fallback）`

- 旧：`→ 条目 15/15: func_o — 继承（路径在范围且函数定义存在）`
- 新：`→ 条目 15/15: func_o — 继承 [c1]`

### SKILL.md §七 使用示例 — 断点续分析

同一次分析的续跑示例，逐条输出也补 tag：

- 旧：`→ 条目 8/15: func_h — 继承`
- 新：`→ 条目 8/15: func_h — 继承 [c1]`

- 旧：`→ 条目 15/15: func_o — 继承`
- 新：`→ 条目 15/15: func_o — 继承 [c1]`

两处 `分析完成。继承: 12 | 淘汰: 3` 汇总行不含 reason，不改。

### test_fixtures/project/.ethunter_out/api-fixer/progress.json

10 条 `results` 按上表重写 reason：`[c1]`×5、`[d1]`×1、`[d2]`×1、`[d3]`×3。

| # | name | original_file | result | path_updated | 新 reason |
|---|------|---------------|--------|--------------|-----------|
| 1 | handle_user_request | core/dispatcher.c | inherited | false | `[c1] 原路径在分析范围内，且函数定义存在` |
| 2 | handler_func_b | core/dispatcher.c | inherited | true | `[d2] 在 scope_files 中找到同名函数定义于 .../comm/msg_handler.c` |
| 3 | handler_func_c | core/dispatcher.c | eliminated | false | `[d3] scope_files 中未找到同名函数定义` |
| 4 | legacy_handler | core/legacy.c | eliminated | false | `[d3] scope_files 中未找到同名函数定义` |
| 5 | duplicate_name | core/dispatcher.c | inherited | false | `[c1] 原路径在分析范围内，且函数定义存在` |
| 6 | duplicate_name | comm/msg_handler.c | eliminated | false | `[d1] 同名函数 duplicate_name 已被其他 old_api 条目继承（文件：.../core/dispatcher.c），跳过 fallback` |
| 7 | process_ipc_message | comm/msg_handler.c | inherited | false | `[c1] 原路径在分析范围内，且函数定义存在` |
| 8 | read_from_shared_memory | comm/msg_handler.c | inherited | false | `[c1] 原路径在分析范围内，且函数定义存在` |
| 9 | handle_ipc_queue | io/ipc_handler.c | inherited | false | `[c1] 原路径在分析范围内，且函数定义存在` |
| 10 | deleted_api | core/removed.c | eliminated | false | `[d3] scope_files 中未找到同名函数定义` |

`phase` / `total` / `processed` 及每条目的 `name`、`original_file`、`file`、`result`、`path_updated` 保持不变；文件中的 `...` 在实际写入时替换为完整绝对路径。

### test_fixtures/TEST_PLAN.md

api-fixer 用例表的"预期结果"列补 tag：

| # | 预期结果 |
|---|---------|
| 1 | inherited, path unchanged, `[c1]` |
| 2 | inherited, path_updated=true, `[d2]` |
| 3 | eliminated, `[d3]` |
| 4 | eliminated, `[d3]` |
| 5 | 1 inherited `[c1]` + 1 eliminated `[d1]` |
| 6 | inherited, `[c1]` |
| 7 | inherited, `[c1]` |
| 8 | inherited, `[c1]` |
| 9 | eliminated, `[d3]` |

"预期 api-fixer inherited_apis.json: 6 个条目"不变。

### 不改动的文件

- `test_fixtures/expected/api-fixer/inherited_apis.json`——只含 `name` + `file`，不受 reason 变更影响。
- `docs/superpowers/specs/2026-08-07-api-fixer-design.md`——记录当时的 `reason = null` 设计，作为历史快照保留。

## 影响范围

- **仅文案变更，无流程或分支变化。** b/c/d 的判定逻辑、进入条件、去重规则全部不变，只是各分支落记录的 reason 取值改变。
- **不影响 `inherited_apis.json`。** 该文件只输出 `name` + `file`。
- **不影响断点恢复机制。** `results` 结构与比对键（`name` + `original_file`）不变。
- **不考虑历史结果兼容。** 已存在的 `progress.json`（含 `[fallback]` / `null`）不做迁移，按既有约定删除后重新分析即可。
