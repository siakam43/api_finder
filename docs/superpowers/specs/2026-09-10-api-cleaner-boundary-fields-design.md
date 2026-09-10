# api-cleaner 新增通信边界与参数索引字段

## 问题

api_clean.json 当前只有 4 个字段（name/file/form/taint_data），下游无法直接获知：
1. 接口所在模块（本模块）名称；
2. 与本模块交互的外部模块名称（通信边界对端）；
3. 外部输入参数在函数签名中的位置（用于后续数据流/污点分析）。

另外，通信边界关联验证（2.4.d）只验证"能否关联"，不记录关联结果；channel_read 分支（2.5）连关联验证都没有，两类接口处理不对称。

## 设计

api_clean.json 从 4 字段扩为 7 字段，新增 3 个，追加在原 4 字段之后：

| 字段 | 类型 | 取值规则 |
|------|------|---------|
| `name` | string | 不变 |
| `file` | string | 不变 |
| `form` | string | 不变 |
| `taint_data` | string | 不变 |
| `local_module` | string | 初始化时从 arch.md 模块概要提取的简洁模块名（如 `"ISP 固件"`），所有条目同一值 |
| `partner_module` | string | arch.md 边界表"外部实体"列中该接口关联行的名称；多条边界用 `/` 分割（如 `"用户态 APP/DDR 控制器"`） |
| `para_index` | string | 与 `taint_data` 逐项对应，`/` 分割：参数项 = 函数签名中 0-based 位置（第一个参数索引为 0，含非外部参数占位）；信道项 = `-1` |

### taint_data 组合顺序规则（新增明确）

form = `both` 时，参数项在前（按签名顺序），信道项在后（按 2.5.a 记录顺序），用 `/` 连接。para_index 与 taint_data 逐项严格对齐，项数必须一致。

### 输出示例

```json
[
  {"name": "isp_process", "file": "/srv/src/isp.c",
   "form": "parameter_input", "taint_data": "data/len",
   "local_module": "ISP 固件", "partner_module": "用户态 APP", "para_index": "1/2"},
  {"name": "shm_handler", "file": "/srv/src/shm.c",
   "form": "channel_read", "taint_data": "共享内存 shm_ptr",
   "local_module": "ISP 固件", "partner_module": "DDR 控制器", "para_index": "-1"},
  {"name": "dual_input", "file": "/srv/src/dual.c",
   "form": "both", "taint_data": "buf/共享内存 shm_ptr",
   "local_module": "ISP 固件", "partner_module": "用户态 APP/DDR 控制器", "para_index": "0/-1"}
]
```

### SKILL.md 各步骤修改点

**Step 6（初始化，新增）：**
- 读取 arch.md"模块概要"章节，提取简洁模块名 → `local_module`，写入 analysis_state.json 每个 entry（同一值）
- 新 entry 的三个字段初始化为 `null`

**2.4.b（锁定外部输入参数，修改）：**
- 遍历签名时记录每个外部输入参数的 0-based 索引（含非外部参数占位，如 `func(int fd, void *data, int len)` 中 data=1、len=2）
- taint_data 记录参数名（不变），参数项索引在 2.7 组装

**2.4.d（通信边界关联验证，修改）：**
- 保留原排除逻辑（无法关联 → 排除）
- 新增：关联成功时记录边界表"外部实体"名称，多条用 `/` 连接 → `partner_module`

**2.5 新增 c 小节（通信边界关联验证）：**
- 与 2.4.d 完全相同的关联验证逻辑（同一段规则文字），对 channel_read 接口执行：无法与任何通信边界明确关联 → 排除；关联成功 → 记录 partner_module
- 信道项索引记为 -1

**2.7（记录结论，修改）：**
- 组装 `para_index`：按 taint_data 项顺序（参数项在前、信道项在后）逐项对齐，参数项填索引、信道项填 -1，`/` 连接
- form = `both` 时：2.4.d 与 2.5.c 的 partner 结果去重合并，`/` 连接
- entry 新增三个字段写入并保存

**4.1（api_clean.json 输出，修改）：**
- 字段表从 4 个扩为 7 个，直接取 analysis_state.json 对应值
- 更新示例 JSON

**4.2（cleaner_summary.md，小改）：**
- 保留接口的"通信边界"一行直接使用记录的 `local_module` → `partner_module` 信息

## 边界情况与规则

- 单项 para_index 也是字符串：`"0"`、`"-1"`（不混用数字类型）
- 纯 channel_read 接口若有参数但未被判定为外部输入（框架参数/输出参数），taint_data 不含参数项，para_index 全为 `-1`
- 被排除的接口不写新字段（保持 null），不进 api_clean.json
- 2.4.d / 2.5.c 关联失败 → 排除，不记录 partner_module
- form = `both` 时 2.4.d 与 2.5.c 都执行，partner 结果合并去重

## 不做的事（YAGNI）

- 不改 api-archreader（local_module 由 cleaner 从模块概要提取）
- 不做旧格式状态文件兼容（已确认无需处理历史结果）
- 不改 progress.json 结构

## 改动范围

仅 `api-cleaner/SKILL.md`：Step 6、2.4.b、2.4.d、2.5（新增 c 小节）、2.7、4.1、4.2，以及约束规则中用词一致性。

## 预期效果

| 场景 | 当前行为 | 修改后行为 |
|------|---------|-----------|
| parameter_input 保留接口 | 输出 4 字段，无参数位置信息 | 输出 7 字段，para_index 标明参数 0-based 索引 |
| channel_read 保留接口 | 输出 4 字段，无边界关联验证 | 输出 7 字段，经 2.5.c 关联验证后记录 partner_module，para_index = -1 |
| both 保留接口 | 输出 4 字段 | taint_data/para_index 逐项对齐（参数项在前、信道项 -1），partner 去重合并 |
| 无法关联边界的 channel_read 接口 | 可能保留 | 与 parameter_input 一致，排除 |
