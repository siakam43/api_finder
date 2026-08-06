# api-finder: old_api 淘汰接口追踪设计

## 目标

在 api-finder 分析流程中，追踪 old_api.json 中每一个历史接口的最终去向：要么继承保留，要么淘汰（含具体淘汰原因）。淘汰记录在 finder_summary.md 末尾汇总输出，实现 old_api 全部接口的"去向可解释"。

## 范围

- **仅追踪 old_api.json 中的接口。** 新识别接口（feature/arch_identify 来源）在第七节被淘汰时不记录。
- old_api 接口可能在两个阶段被淘汰：
  - **第四节（接口继承）**：文件不在 scope、或函数定义不存在
  - **第七节（外部接口筛选）**：黑名单、测试函数、无外部输入、冗余函数、校验失败

---

## 新增数据结构

### `tmp/old_api_fate.json`

追踪 old_api.json 每一个条目的完整去向。第四节创建，第七节按需更新。如果 old_api.json 不存在，第四节跳过，不创建本文件。输出阶段检查本文件是否存在，不存在则跳过附节。

```json
[
  {
    "name": "FUNC_NAME",
    "file": "/abs/path/to/file.c",
    "fate": "inherited",
    "stage": null,
    "reason": null
  },
  {
    "name": "FUNC_NAME2",
    "file": "/abs/path/to/file.c",
    "fate": "eliminated",
    "stage": "inherit",
    "reason": "Step 2a：文件 /old/path/file.c 不在本次 scope_files 分析范围内"
  },
  {
    "name": "FUNC_NAME3",
    "file": "/abs/path/to/file.c",
    "fate": "eliminated",
    "stage": "filter",
    "reason": "规则二：无外部输入 — 函数三个参数均为 VFS 框架内部参数（struct file*、struct inode*），不携带外部数据"
  }
]
```

字段：
- `name` / `file` — 直接来自 old_api.json，`file` 保持 old_api.json 中的原始值
- `fate` — `"inherited"`（最终保留）或 `"eliminated"`（被淘汰）
- `stage` — 淘汰阶段：`"inherit"` 或 `"filter"`。`fate = "inherited"` 时为 null
- `reason` — 淘汰原因，确凿证据标准（与第七节要求一致）

### `tmp/filter_state.json` 新增字段

api_list 每个条目新增 `origin` 字段：

```json
{
  "name": "func",
  "file": "/path/to/file.c",
  "origin": "inherit",
  "reviewed": false,
  "decision": null,
  "reason": null,
  "validated": false
}
```

`origin` 取值：`"inherit"` / `"feature"` / `"arch_identify"`，在合并阶段根据接口来源文件赋值。

---

## 各阶段改动

### 第四节（接口继承）

对 old_api.json 中**每一个条目**（不仅是验证通过的）记录去向：

| 验证结果 | fate | stage | reason |
|---------|------|-------|--------|
| 两步都通过 | `"inherited"` | `null` | `null` |
| Step 2a 失败 | `"eliminated"` | `"inherit"` | `"Step 2a：文件 <file> 不在本次 scope_files 分析范围内"` |
| Step 2b 失败 | `"eliminated"` | `"inherit"` | `"Step 2b：在文件 <file> 中未找到函数 <name> 的定义（可能已删除、重命名或移至其他文件）"` |

全部条目验证完成后，同时写入：
- `inherited_apis.json` — 仅 `fate = "inherited"` 的条目（格式不变，供第五节和后续使用）
- `tmp/old_api_fate.json` — 全部条目（新文件）

### 第七节（外部接口筛选）

**合并阶段**：生成 api_pool 时，每条标注 `origin`：

| 来源文件 | origin 值 |
|---------|----------|
| inherited_apis.json | `"inherit"` |
| feature_apis.json | `"feature"` |
| arch_apis.json | `"arch_identify"` |

**逐条审查和校验阶段**：逻辑不变。

**新增回写步骤**（在逐条审查和分析范围校验全部完成后执行）：

```
for each entry in api_list:
    if entry.origin == "inherit" AND entry.decision == "exclude":
        在 old_api_fate.json 中按 name 匹配对应条目
        将 entry 的 fate 从 "inherited" 改为 "eliminated"
        将 stage 设为 "filter"
        将 reason 改为 entry.reason
        将 file 更新为 entry.file（校验阶段可能已更新文件路径）
        保存 old_api_fate.json
```

匹配用 `name` 而非 `name + file`，因为第七节"分析范围校验"可能更新 `file` 字段（函数在另一文件中找到定义），此时用原 `file` 匹配会失败。old_api.json 中函数名本身唯一，`name` 匹配已足够。同时回写时同步更新 `file`，确保淘汰记录中的文件路径反映最新实际位置。

注意：`decision = "keep"` 的继承接口在"分析范围校验"阶段可能被改为 `"exclude"`，因此回写必须在校验阶段完成后执行。

### 输出阶段（finder_summary.md）

在现有 finder_summary.md 正文之后，追加新章节。读取 `old_api_fate.json`，统计并输出。

**统计计算：**
- `total = old_api_fate.json 条目数`
- `inherited_count = fate == "inherited" 的条目数`
- `eliminated_count = fate == "eliminated" 的条目数`
- `inherit_stage_eliminated = fate == "eliminated" AND stage == "inherit" 的条目`
- `filter_stage_eliminated = fate == "eliminated" AND stage == "filter" 的条目`

**输出格式：**

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

**<序号>. <函数名> — <原文件路径>**
- **淘汰阶段：** 接口继承
- **淘汰原因：** Step 2b：在文件 /path/to/file.c 中未找到函数 func_foo 的定义（可能已删除、重命名或移至其他文件）

#### 第七节（外部接口筛选阶段）淘汰 — W 个

**<序号>. <函数名> — <文件路径>**
- **淘汰阶段：** 外部接口筛选
- **淘汰原因：** 规则二：无外部输入 — 函数的 data 参数经追踪为内核内部缓冲区，不来自外部不可信源

**<序号>. <函数名> — <文件路径>**
- **淘汰阶段：** 外部接口筛选
- **淘汰原因：** 分析范围校验失败：函数定义未找到
```

统计校验约束：`Z + W = Y`，`X + Y = N`。
`fate = "inherited"` 的接口已在 finder_summary.md 正文中逐条详述，附节不重复。

---

## 断点续分析

淘汰记录是第四、第七节的**副产品**（追加式日志），不引入独立的状态文件和断点恢复机制。断点续分析由各节自身状态文件保证：

- 第四节无状态文件（原子操作，一次性完成）
- 第七节由 `filter_state.json` 保证断点恢复

如果分析在第七节回写 `old_api_fate.json` 时中断，恢复后第七节从断点继续审查，全部完成后再统一回写一次即可（回写幂等：按 name 匹配后覆写 reason）。
