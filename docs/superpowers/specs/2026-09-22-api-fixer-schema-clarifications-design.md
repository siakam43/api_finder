# api-fixer 提示词歧义澄清设计

## 问题

reason tag 化完成后通读 `api-fixer/SKILL.md`，发现 8 处歧义会导致不同执行者跑出不同结果或搜索范围超出预期。逐条判定后修 5 处，保留 3 处。

## 判定与决策

| # | 位置 | 问题 | 决策 |
|---|------|------|------|
| 1 | §二 结构示例 + 字段说明 + §三 d1/d3 | 淘汰条目的 `file` 取什么值，全文未定义；`约束规则 5` 又要求一律绝对路径 | **淘汰条目 `file = null`**，继承条目仍为绝对路径 |
| 2 | §二 字段说明 | 仅发生 a 步路径规范化（相对→绝对）时 `path_updated` 取值未明确 | **不算更新，仍记 `false`**（`true` 仅限 fallback 搜索命中） |
| 3 | §三 d2 | 「取第一个匹配的文件」无 tie-break 规则 | **不规定**。极少发生，交由执行者自行判断 |
| 4 | §三 d 的 fallback 搜索 | 字面范围是整个 `<project_dir>`，会把 `old_api.json` 与历史结果文件扫进上下文 | **搜索时跳过 `<project_dir>/.ethunter_out/` 目录** |
| 5 | §一 Step 2 情况 B/C | `find` 不排除输出目录，会把 `.ethunter_out/` 下的 .c/.h 收进 scope_files | **`find` 加 `-not -path "*/.ethunter_out/*"`** |
| 6 | §二 入口恢复流程 第 4 条 | 「跳到 Step 3（输出结果）」跨节编号歧义（§一 也有 Step 3） | **写明「第三节的 Step 3」** |
| 7 | §三 d1/d2 模板 | `<已有条目file>` / `<新路径>` 是绝对还是相对路径未写 | **不改**。纯文案，不影响判定 |
| 8 | §一 Step 4 | 用 `Read` 工具检查 `.codegraph` 目录是否存在，对目录无效 | **不改**。执行者可自行换用 `ls`/`find` |

## 变更清单（`api-fixer/SKILL.md`）

### §一 Step 2 — find 命令（情况 B、情况 C 两处）

- 旧：`find <project_dir> -type f \( -name "*.c" -o -name "*.h" \)`
- 新：`find <project_dir> -type f \( -name "*.c" -o -name "*.h" \) -not -path "*/.ethunter_out/*"`

### §二 progress.json 结构示例

- 旧：`    {"name": "func_a", "original_file": "old_api原始值", "file": "最终绝对路径",`
- 新：`    {"name": "func_a", "original_file": "old_api原始值", "file": "最终绝对路径|null",`

### §二 字段说明 — file

- 旧：``- `file`：最终确定的文件绝对路径``
- 新：``- `file`：最终确定的文件绝对路径；淘汰条目为 `null```

### §二 字段说明 — path_updated

- 旧：``- `path_updated`：`true` 表示路径在 fallback 搜索中被更新，`false` 表示路径未变``
- 新：``- `path_updated`：`true` 表示路径在 fallback 搜索中被更新，`false` 表示路径未变。a 步的路径规范化（相对路径转绝对路径）不算更新，仍记 `false```

### §二 入口恢复流程 第 4 条

- 旧：`4. 待处理条目为空 → 全部已处理，跳到 Step 3（输出结果）`
- 新：`4. 待处理条目为空 → 全部已处理，跳到第三节的 Step 3（输出结果）`

### §三 Step 2d — d1 记录块

- 旧：`     │     result = "eliminated", path_updated = false,`
- 新：`     │     result = "eliminated", path_updated = false, file = null,`

### §三 Step 2d — fallback 搜索范围

- 旧：`           使用"搜索函数定义的方法"在 <project_dir> 中搜索，并只保留文件路径在 scope_files 中的匹配结果。`
- 新：
  ```
             使用"搜索函数定义的方法"在 <project_dir> 中搜索（跳过 <project_dir>/.ethunter_out/ 目录，
             避免把 old_api.json 和历史结果文件扫入上下文），并只保留文件路径在 scope_files 中的匹配结果。
  ```

### §三 Step 2d — d3 记录块

- 旧：`                 result = "eliminated", path_updated = false,`
- 新：`                 result = "eliminated", path_updated = false, file = null,`

### §四 约束规则 5

- 旧：`5. **文件绝对路径。** inherited_apis.json 和 progress.json 中的 file 字段一律使用绝对路径。`
- 新：`5. **文件绝对路径。** inherited_apis.json 和 progress.json 中继承条目的 file 字段一律使用绝对路径；淘汰条目为 `null`。`

### test_fixtures/project/.ethunter_out/api-fixer/progress.json

4 条淘汰条目（`handler_func_c`、`legacy_handler`、`duplicate_name`/comm、`deleted_api`）的 `file` 由原路径改为 `null`。其余字段与 10 条 reason 全部不变。

## 影响范围

- **`progress.json` 的形状变化：** 淘汰条目的 `file` 由「原路径」变为 `null`。断点恢复的比对键是 `name` + `original_file`（§二 入口恢复流程 第 3 条），不读 `file`，故恢复机制不受影响。
- **`inherited_apis.json` 不受影响。** 该文件只输出 `result = "inherited"` 的条目，其 `file` 仍为绝对路径。
- **不改流程与分支。** 5 处改动均为取值定义、搜索范围与措辞，a→b→c→d 的判定逻辑不变。
- **不考虑历史兼容。** 改动前录制的 `progress.json` 中淘汰条目带路径，不做迁移。
