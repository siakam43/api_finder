# api-cleaner C 语法属性中立性 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 api-cleaner 明确"函数是否被 `static` 修饰、链接属性如何"不构成排除依据（也不构成保留依据），堵住 cleaner 因"误把 `static` 当内部实现证据"而直接排除接口的风险。

**Architecture:** 纯文档修改，单文件 `api-cleaner/SKILL.md`。两处改动：在 §五 约束规则 之后新增「六、注意事项」章节（原六/七/八 顺延为 七/八/九），「抗理性化检查」表补一行。不改任何流程分支、判定规则与输出格式。

**Tech Stack:** Markdown

**Spec:** `docs/superpowers/specs/2026-09-22-api-cleaner-static-neutrality-design.md`

**验证方式：** 本改动是**预防性**的——api-cleaner 尚未观察到 static 误排实例，无 bug 可复现。验证分两层：

1. **文案层（主）：** 全文 `static` 仅命中新增两处；章节顺序正确；末尾 `2.4 / 2.5` 引用指向真实小节。
2. **回归层（辅）：** 实跑一次 `/api-cleaner test_fixtures/project`，确认 `api_clean.json` 与项目内录制基线一致（5 保留）。

---

### Task 1: 新增「六、注意事项」并顺延后续三个标题

**Files:**
- Modify: `api-cleaner/SKILL.md:538-542`（§五 约束规则 末尾至原「六、权限申请」）
- Modify: `api-cleaner/SKILL.md:558`、`:573`（另两个待顺延标题）

- [ ] **Step 1: 插入「六、注意事项」并把「六、权限申请」改为「七、」**

将：

```
10. **适用业务。** 本 skill 适用于嵌入式/底层系统代码（Linux 内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU 固件）。

---

## 六、权限申请
```

替换为：

```
10. **适用业务。** 本 skill 适用于嵌入式/底层系统代码（Linux 内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU 固件）。

---

## 六、注意事项

以下事项适用于全部阶段，无论在哪一步做判断都成立。

1. **C 语法属性不参与接口判定。** 函数是否被 `static` 修饰、链接属性是内部链接还是外部链接，既不构成排除依据，也不构成保留依据——不因被 `static` 修饰而排除，也不因其未被 `static` 修饰而认定。判定只依据 2.4 / 2.5 的外部输入与通信边界分析。

---

## 七、权限申请
```

新增条目写成**单行**（不折行），与 §五 约束规则各条排版一致。已有的 `---` 保留在新章节之前，新章节与「七、权限申请」之间补一个 `---`。

- [ ] **Step 2: 顺延另两个标题**

将：

```
## 七、抗理性化检查
```

替换为：

```
## 八、抗理性化检查
```

将：

```
## 八、使用示例
```

替换为：

```
## 九、使用示例
```

全文没有按编号的交叉引用，顺延只改标题本身。

- [ ] **Step 3: 验证并提交**

用 Read 重读 `api-cleaner/SKILL.md` 的 536-548 行与 556-578 行。核对点：

- 章节顺序为 `……五、约束规则 → 六、注意事项 → 七、权限申请 → 八、抗理性化检查 → 九、使用示例`
- 注意事项正文为：引导句「以下事项适用于全部阶段，无论在哪一步做判断都成立。」+ 单条编号 1（单行）
- 相邻两节之间均有 `---` 分隔
- 权限申请、抗理性化检查、使用示例三节的内容除标题编号外未变
- `grep -c 'static' api-cleaner/SKILL.md` 为 **1**（仅注意事项第 1 条）
- 全文仍无按编号的交叉引用（`grep -n '第[一二三四五六七八九十]节' api-cleaner/SKILL.md` 无输出）

```bash
git add api-cleaner/SKILL.md
git commit -m "docs(api-cleaner): add a global notes section on syntax-attribute neutrality"
```

---

### Task 2: 「抗理性化检查」表追加一行

**Files:**
- Modify: `api-cleaner/SKILL.md`（顺延后的「八、抗理性化检查」表末）

- [ ] **Step 1: 追加一行**

将：

```
| "这个状态文件检查结果不太确定，先继续吧" | 文件存在性检查必须严格遵循双方法+判断规则。 |
```

替换为：

```
| "这个状态文件检查结果不太确定，先继续吧" | 文件存在性检查必须严格遵循双方法+判断规则。 |
| "这个函数被 static 修饰，应该是内部实现" | C 语法属性不参与判定，被 static 修饰不构成排除依据。 |
```

该行原为表格最后一行，追加后仍为最后一行。

- [ ] **Step 2: 验证并提交**

用 Read 重读 `api-cleaner/SKILL.md` 的「八、抗理性化检查」整节。核对点：

- 新行是表格末行，2 列，`|` 数量与其余 6 行一致
- `grep -c 'static' api-cleaner/SKILL.md` 为 **2**（注意事项 1 行 + 抗理性化 1 行）
- 念头句写的是「应该是内部实现」（过度排除方向），不是 api-finder 那句「不像外部接口」（保留侧犹疑）——两个 skill 的风险方向不同，措辞必须区分

```bash
git add api-cleaner/SKILL.md
git commit -m "docs(api-cleaner): guard against a static-based exclusion in the anti-rationalization table"
```

---

### Task 3: 全文一致性核对与回归验证

**Files:**
- Review: `api-cleaner/SKILL.md`（全文）

- [ ] **Step 1: 重读全文，逐项核对**

用 Read 通读整个 `api-cleaner/SKILL.md`（约 620 行），核对：

1. **中立性声明无歧义。** 注意事项第 1 条同时排除了"排除依据"与"保留依据"两个方向；全文无"static 函数应视为内部实现"之类的反向表述。
2. **与核心原则第 2 条不矛盾。** 核心原则第 2 条是"更少的误报优先于更少的漏报，不确定时倾向于排除"；注意事项取消的是"`static`"这个本不该参与判定的维度，不是放宽证据标准——即：命中注意事项后，接口仍要过 §2.2 / §2.3 / §2.4 / §2.5 全部分析，只是不得因链接属性而被排除。
3. **与 §2.4.a 内部调用者检查不矛盾。** §2.4.a 的"存在任何普通函数调用 → 直接排除"依据的是**引用点形态**（调用 vs 注册），不是函数的链接属性。确认注意事项没有为该规则提供豁免，也没有与之冲突。
4. **末句引用可达。** 「判定只依据 2.4 / 2.5 的外部输入与通信边界分析」中的 `2.4` / `2.5` 指向真实小节（`### 2.4 parameter_input 深入分析`、`### 2.5 channel_read 深入分析`）。
5. **无残留。** 全文 `static` 仅出现在新增两处；无 `static` / 链接属性相关的旧表述需要清理。

如发现不一致，直接修复并重新核对。

- [ ] **Step 2: 回归实跑**

按计划末尾「验收标准」执行。

- [ ] **Step 3: 最终提交**

```bash
git add api-cleaner/SKILL.md
git commit -m "docs(api-cleaner): final consistency pass for syntax-attribute neutrality"
```

（若 Step 1 无改动，跳过本次提交，报告即可。spec `docs/superpowers/specs/2026-09-22-api-cleaner-static-neutrality-design.md` 已在 brainstorming 阶段提交，本次不重复提交。）

---

## 验收标准（全部任务完成后）

对 `test_fixtures/project` 实跑一次 `/api-cleaner`，确认未引入回归。

**基线说明（重要）：** api-cleaner 有两套 api_clean.json：

- `test_fixtures/project/.ethunter_out/api-cleaner/` — **本次比对的基线**。7 字段格式，5 保留 / 2 排除（排除 `handle_system_event`、`read_device_status`）。
- `test_fixtures/expected/api-cleaner/baseline-2026-09-10/` — commit 777e96f 明确标注为 **"RED baseline"**，是改动**前**的录制快照（4 字段旧格式、7 保留）。**不要拿它当验收标准。**

1. 备份基线：

   ```bash
   cp test_fixtures/project/.ethunter_out/api-cleaner/api_clean.json /tmp/api-cleaner-baseline.json
   cp test_fixtures/project/.ethunter_out/api-cleaner/progress.json /tmp/api-cleaner-progress.baseline.json
   ```

2. 删除 `test_fixtures/project/.ethunter_out/api-cleaner/progress.json`。**只删这一个即可**：cleaner 的 Step 6 分支写明"progress.json 确认不存在 → 全新分析，将 api.json 中所有接口写入 analysis_state.json，全部 status = pending"，因此 `tmp/analysis_state.json` 会被重新初始化，无需删除。

3. 派子代理按修改后的 `api-cleaner/SKILL.md` 执行 `/api-cleaner test_fixtures/project`。子代理须完整读取 SKILL.md 后按其流程执行，且**不得读取** `test_fixtures/expected/`、`test_fixtures/TEST_PLAN.md`、`docs/superpowers/`，也不得读取历史 `api_clean.json` / `cleaner_summary.md`（会被本次运行覆盖）。

4. 比对实跑产出的 `api_clean.json` 与 `/tmp/api-cleaner-baseline.json`，**逐条目 `name` + `file` 一致**。基线为 5 条：
   ```
   handler_func_b / process_ipc_message / read_from_shared_memory / handle_ipc_queue / dispatch_ioctl
   ```
   注意本 skill 有判断裁量空间（`handle_system_event` 的边界关联、`read_device_status` 的单整数排除都带推理成分），若出现条目级差异，先判定是模型执行偏差还是文案缺陷，**不得直接把差异改基线迁就**。

5. 比对通过后把实跑产出恢复基线（`git checkout -- <文件>`），避免留下格式噪音。

**本次期望结果：与基线零差异。** 本改动不改变任何排除判定。若差异涉及 `static` / 链接属性，说明文案未奏效，回到 Task 3 Step 1 重新核对措辞。

**复现性说明：** api-cleaner 并未观察到 static 误排，实跑通过**不构成**"bug 已修复"的证据，只证明未引入回归；中立性效果的直接依据是 Task 3 Step 1 的文案核对。
