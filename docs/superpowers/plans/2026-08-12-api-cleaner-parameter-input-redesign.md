# api-cleaner 2.4 parameter_input 逻辑重构 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 重构 api-cleaner/SKILL.md 中 2.4.b 和 2.4.c 两节，将整数判断从 b 移到 c，b 只做筛除（框架/输出），c 先 count 再判类型。

**Architecture:** 纯文档修改，单文件 `api-cleaner/SKILL.md`，只改两处文本块。不改任何代码逻辑或工具。

**Tech Stack:** Markdown

---

### Task 1: 重写 2.4.b（逐参数外部输入判断）

**Files:**
- Modify: `api-cleaner/SKILL.md:279-293`

- [ ] **Step 1: 替换 2.4.b 整节内容**

将第 279-293 行：

```markdown
#### b. 逐参数外部输入判断

对函数签名中的每个入参逐一分析，判断其是否为外部输入：

对每个参数回答以下问题：
1. 该参数是系统框架类参数吗？（`struct file *f`、`struct inode *i`、内核内部数据结构指针、框架回调约定的上下文参数等）→ 是 → **内部参数，跳过**
2. 该参数用于输出目的吗？（`void *out_buffer`、`int *result`、`void *ret_data`），代码中对该参数仅写入不读取 → 是 → **内部参数，跳过**
3. 该参数是指针/缓冲区/结构体指针吗？（`void *data`、`char *buf`、自定义结构体指针），代码中有对该指针指向内容的读取操作 → 是 → **外部输入参数**
4. 该参数是整数型吗？→ 是 → 进入 c. 判断

记录外部输入参数列表。如果列表为空 → **排除**（无外部输入）。

将识别出的外部输入参数名记录到 `taint_data` 字段，多个参数名用 `/` 分割。例如：`"data/len"`（两个参数都是外部输入）、`"buf"`（仅 buf 是外部输入）。

**重要：** 大多数情况下，外部数据以指针形式传递。判断时要仔细分析每个参数的实际用途，不要看到有参数就认为有外部输入。
```

替换为：

```markdown
#### b. 锁定外部输入参数列表

对函数签名中的每个入参逐一分析，筛选出外部输入参数：

对每个参数回答以下两个问题：
1. 该参数是系统框架类参数吗？（`struct file *f`、`struct inode *i`、内核内部数据结构指针、框架回调约定的上下文参数等）→ 是 → **内部参数，跳过**
2. 该参数用于输出目的吗？（`void *out_buffer`、`int *result`、`void *ret_data`），代码中对该参数仅写入不读取 → 是 → **内部参数，跳过**

两个都否 → 该参数为**外部输入参数**，加入外部输入参数列表。

统计外部输入参数列表：
- 个数 = 0 → **直接排除**（无外部输入），记录 reason
- 个数 > 0 → 记录参数名到 `taint_data` 字段（多个以 `/` 分割），进入 c. 外部输入参数个数与类型裁决

`taint_data` 示例：`"data/len"`（两个参数都是外部输入）、`"buf"`（仅 buf 是外部输入）。
```

- [ ] **Step 2: 验证替换后文件完整性**

```bash
grep -n "#### b." api-cleaner/SKILL.md
grep -n "#### c." api-cleaner/SKILL.md
grep -n "#### d." api-cleaner/SKILL.md
```

确认三节标题连续存在，无错位。

- [ ] **Step 3: Commit**

```bash
git add api-cleaner/SKILL.md
git commit -m "$(cat <<'EOF'
docs(api-cleaner): simplify 2.4.b to only filter framework/output params

Remove pointer-type and integer-type questions from per-parameter loop.
b now only does two checks per parameter, producing a clean external
input parameter list for c to process.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: 重写 2.4.c（整数型参数特殊处理）

**Files:**
- Modify: `api-cleaner/SKILL.md:295-308`

- [ ] **Step 1: 替换 2.4.c 整节内容**

将第 295-308 行：

```markdown
#### c. 整数型参数特殊处理

如果所有外部输入参数**仅有一个整数型参数（包括枚举、布尔类型）**（其他参数都是内部参数），需要判断该整数是否为内存地址：

以下情况视为**内存地址，保留**：
- 参数类型为 `uintptr_t`、`unsigned long`、`size_t` 等，且在函数体内被显式强转为指针类型（如 `(void *)arg`、`(struct foo *)arg`）
- 参数在函数体内被用于内存访问操作：解引用（`*ptr`）、传给 `memcpy`/`memset`/`copy_from_user`/`mmap` 等内存操作函数的地址参数位置

以下情况视为**普通整数值，排除**（安全影响小）：
- 参数仅作为计数器、标志位（flags）、长度（len/size）使用
- 参数仅用于算术运算或比较操作
- 参数是枚举值、命令码（cmd/opcode）等

**判断要仔细：** 即使参数名是 `addr`/`address`/`ptr`/`base`，也要看代码中实际如何使用。类型为 `unsigned long` 时尤其要注意——在 64 位系统中 `unsigned long` 常被用作指针存储类型。
```

替换为：

```markdown
#### c. 外部输入参数个数与类型裁决

先统计 b 产出的外部输入参数列表个数，再决定是否做类型判断。**类型判断只看参数本身的 C 声明类型，绝不追踪函数体内从 struct/union 取出的成员类型：**

- `struct req *r` → 参数本身是指针，不是整数型，即使函数体内使用了 `r->cmd_id`
- `void *data` → 参数本身是指针，不是整数型
- `int fd` → 参数本身是整数型

1. **个数 > 1**：多个外部输入参数，安全影响显著 → **无需额外判断，保留接口**

2. **个数 = 1**：唯一的外部输入参数，需判断其类型：
   - **非整数型**（指针、结构体指针、缓冲区、数组等）→ **保留接口**。指针类型本身就是外部数据的入口载体，无需进一步验证
   - **整数型**（含枚举、布尔）→ 需要判断该整数是否为内存地址：
     - 是内存地址 → **保留**
     - 非内存地址（仅作计数器、标志位、长度、命令码等）→ **排除**

3. **内存地址判断规则**：
   - 视为**内存地址，保留**：参数类型为 `uintptr_t`、`unsigned long`、`size_t` 等，且在函数体内被显式强转为指针类型（如 `(void *)arg`、`(struct foo *)arg`）；或被用于解引用（`*ptr`）、传给 `memcpy`/`memset`/`copy_from_user`/`mmap` 等内存操作函数的地址参数位置
   - 视为**普通整数值，排除**（安全影响小）：仅作为计数器、标志位（flags）、长度（len/size）；仅用于算术运算或比较操作；是枚举值、命令码（cmd/opcode）等
   
   **判断要仔细：** 即使参数名是 `addr`/`address`/`ptr`/`base`，也要看代码中实际如何使用。类型为 `unsigned long` 时尤其要注意——在 64 位系统中 `unsigned long` 常被用作指针存储类型。
```

- [ ] **Step 2: 验证最终文件结构**

```bash
grep -n "####" api-cleaner/SKILL.md | head -20
```

确认 2.4 节下的子标题顺序为：a → b → c → d，且 b 标题为"锁定外部输入参数列表"，c 标题为"外部输入参数个数与类型裁决"。

- [ ] **Step 3: 验证 c 不再引用"仅有一个整数型参数"的旧逻辑**

```bash
grep -n "仅有一个整数" api-cleaner/SKILL.md
```

预期：无匹配（旧逻辑已移除）。

- [ ] **Step 4: Commit**

```bash
git add api-cleaner/SKILL.md
git commit -m "$(cat <<'EOF'
docs(api-cleaner): restructure 2.4.c to count-first type verdict

Replace the old "only one integer param" logic with explicit count-before-type
ordering: count>1 keeps the interface, count=1 checks the param's own C type
(not struct member types extracted in body), integer-only triggers address check.
Prevents struct-pointer params from being mis-excluded.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```
