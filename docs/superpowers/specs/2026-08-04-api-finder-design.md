# api-finder Skill 设计文档

## 概述

api-finder 是一个纯 markdown 的 Claude Code skill，用于在嵌入式/底层 C 项目中识别对外部暴露的接口函数。适用业务场景：Linux 内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU 固件。

**核心原则：**

1. **分析质量优先于分析效率。** 宁可慢，不可草率。每批文件必须完整深入分析，不得因批次多而加速跳读。
2. **更少的误报优先于更少的漏报。** 不确定是否为外部接口时，倾向于排除而非保留。只在有充分证据（代码模式 + 架构信息双重支撑）时才认定为外部接口。

---

## 一、整体架构与数据流

### 目录结构

```
api-finder/
└── SKILL.md              # skill 全部内容

# 运行时在待分析项目中生成：
project_dir/.ethunter_out/api-finder/
├── arch.md               # 项目架构分析
├── progress.json          # 断点续分析状态（轻量顶层追踪）
├── api.json               # 最终输出：外部接口列表
├── summary.md             # 最终输出：识别理由
├── conf/
│   ├── old_api.json       # (可选) 历史外部接口
│   └── black_api.json     # (可选) 历史黑名单
└── tmp/
    ├── inherited_apis.json    # 步骤3中间结果：继承接口
    ├── feature_apis.json      # 步骤4中间结果：特征识别接口
    ├── arch_apis.json         # 步骤5中间结果：架构识别接口
    ├── feature_state.json     # 步骤4状态文件（断点恢复）
    ├── arch_identify_state.json # 步骤5状态文件（断点恢复）
    └── filter_state.json      # 步骤6状态文件（断点恢复）
```

### 主流程

```
初始化(确定范围+加载配置) → 任务恢复检查 → 了解项目架构
  → 接口继承(old_api) → 接口特征提取(分两子阶段) → 架构识别接口(按文件分批)
  → 外部接口筛选 → 最终输出
```

### progress.json 结构

轻量顶层追踪，仅记录各阶段状态和专属状态文件指针：

```json
{
  "phase": "arch_analysis|inherit|feature|arch_identify|filter|done",
  "arch_analysis": { "status": "pending|in_progress|completed" },
  "inherit": { "status": "pending|in_progress|completed" },
  "feature": {
    "status": "pending|in_progress|completed",
    "state_file": "tmp/feature_state.json"
  },
  "arch_identify": {
    "status": "pending|in_progress|completed",
    "state_file": "tmp/arch_identify_state.json"
  },
  "filter": {
    "status": "pending|in_progress|completed",
    "state_file": "tmp/filter_state.json"
  }
}
```

轻量阶段（arch_analysis、inherit）无专属状态文件；复杂阶段（feature、arch_identify、filter）通过 state_file 指向细节。

---

## 二、分析范围确定

```
输入: /api-finder <project_dir>

Step 1: project_dir 默认为当前目录

Step 2: 检查 project_dir/.ethunter_out/
  ├── 存在 clean_code.txt → 按行读取文件路径，作为分析范围
  ├── 不存在 clean_code.txt 但存在 .etignore → 收集 project_dir 下全部 .c/.h，
  │     按 .etignore 语法排除
  └── 两者都不存在 → 收集 project_dir 下全部 .c/.h

Step 3: 如果 project_dir/.ethunter_out/macro.json 存在，
  LLM 了解该文件记录了编译选项中的宏定义（注意：不是全部宏，仅编译指令中
  通过 -D 等选项动态定义的宏），在后续分析中遇到宏判断需求时，除了在代码中
  追溯定义，也到 macro.json 中查找。不需要预先完整读取。如果某个宏在代码
  和 macro.json 中都找不到定义，默认进行分析（不排除该分支代码）。

Step 4: 检查 project_dir/.codegraph
  ├── 存在 → 后续代码分析优先使用 codegraph MCP 工具
  └── 不存在或环境未安装/未配置 MCP → 使用常规工具（grep/find/Read/ast-grep）

Step 5: 初始化输出目录
  mkdir -p project_dir/.ethunter_out/api-finder/conf
  mkdir -p project_dir/.ethunter_out/api-finder/tmp
```

---

## 三、任务恢复机制

### 文件存在性检查规则

**禁止使用 `test -f` 或 `[ -f ]` 判断文件是否存在。** 对每个关键状态文件，必须使用至少两种不同方法交叉确认。

推荐方法：

| 方法 | 示例 |
|------|------|
| `ls` 检查 | `ls <file_path>` |
| `find` 检查 | `find <dir> -name "<filename>" -maxdepth 1` |
| `Read` 工具 | 直接尝试 Read 读取文件 |

**判断规则：**

| 两次检查结果 | 处理方式 |
|-------------|---------|
| 都确认文件存在 | 文件存在，正常恢复 |
| 都确认文件不存在（明确报错，如 "No such file"、"NOT_FOUND"） | 文件不存在，正常跳过/新建 |
| 一次存在、一次不存在 | 存在疑似，再检查第三次，取多数结果 |
| 两次都无输出或都报错 | **停止分析，报告用户**。不要假设文件缺失后继续 |

### 入口恢复流程

```
Step 0: 恢复入口
  1. 用两种方法检查 progress.json 是否存在
  2. 根据判断规则确定文件状态：
     ├── 确认不存在 → 全新分析，从 arch_analysis 开始
     └── 确认存在 → 读取 progress.json，找到当前 phase

Step 0a: 如果 phase 指向复杂阶段（feature/arch_identify/filter）：
  用同样规则检查对应的 state_file 是否存在：
    ├── 确认存在 → 加载状态，从断点继续
    └── 确认不存在（明确报错）→ 该阶段状态文件丢失，退回重做该阶段
```

---

## 四、了解项目架构（arch_analysis）

轻量阶段，但输出质量决定后续所有步骤的识别效果。

### 进入检查

```
1. 用两种方法检查 arch.md 是否存在
2. 确认存在 → 跳过本阶段
3. 确认不存在 → 执行架构分析
```

### 分析步骤

1. 浏览分析范围内的关键文件：头文件（`.h`）优先，了解模块对外暴露的类型和接口；再读核心 `.c` 文件了解实现
2. 结合文件名、目录名、代码注释、函数命名惯例，分析以下内容：
   - **模块概要**：功能定位、在系统中的角色（内核驱动/UEFI固件/XLoader/固件组件等）
   - **外部通信边界**：本模块与哪些外部模块/外部世界有通信、通信信道是什么（系统调用、共享内存、IPC消息队列、网络socket、硬件寄存器/MMIO等）
   - **代码分区映射**：按功能或通信方向划分文件目录，标注哪些文件最有可能包含外部接口，**整理成后续步骤需要深入分析的任务文件列表。该列表必须在分析范围（第二步确定）之内，将作为 arch_identify 阶段的任务清单，确保分析完整不遗漏**
3. 输出到 `arch.md`，更新 progress.json

### arch.md 模板

```markdown
# 项目架构分析 — <project_name>

## 模块概要
本项目模块是 <功能定位>，在系统中扮演 <角色> 角色。
主要职责包括：<列举>。

## 外部通信边界
| 外部实体 | 通信方向 | 通信信道 | 说明 |
|---------|---------|---------|------|
| 用户态APP | 外部→本模块 | 系统调用/ioctl | ... |
| 其他内核模块 | 双向 | 共享内存 | ... |

## 代码分区映射

### 核心通信文件（待深入分析）
- src/comm/a.c — <说明，为何认为该文件包含外部接口>
- src/ipc/b.c — <说明>
...

### 内部实现文件（低优先级，但也需覆盖）
- src/util/c.c — <说明>
...
```

---

## 五、接口继承（inherit）

轻量阶段。将历史外部接口与当前分析范围做交叉比对。

### 流程

```
1. 用两种方法检查 old_api.json 是否存在
   ├── 确认不存在 → 跳过本阶段，写入空的 inherited_apis.json（内容为 []），
   │     更新 progress.json，进入下一阶段
   └── 确认存在 → 继续

2. 读取 old_api.json，逐一检查每个接口，分两步验证：
   a. 接口所在文件是否在本次分析范围内
      ├── 不在分析范围 → 排除（文件可能已删除或不在本次范围）
   b. 在分析范围的，进一步在该文件中搜索函数定义是否依然存在
      （使用 grep 搜索函数定义，确认未被删除或重命名）
      ├── 函数定义不存在 → 排除
      └── 函数定义存在 → 计入继承接口列表

3. 输出 tmp/inherited_apis.json，格式与 old_api.json 一致
```

### tmp/inherited_apis.json 格式

```json
[
  {"name": "FUNC_NAME", "file": "FILE_PATH"},
  ...
]
```

---

## 六、接口特征提取（feature）

复杂阶段。从继承接口中提取注册特征，在全代码范围内推广匹配。

**前置条件：** `inherited_apis.json` 不为空。如果为空，跳过本阶段。

### 流程

```
1. 检查 inherited_apis.json 是否为空
   └── 空 → 跳过本阶段，更新 progress.json，进入下一阶段

2. 进入检查 feature_state.json（两种方法确认存在）
   ├── 确认存在 → 从断点恢复
   └── 确认不存在 → 全新启动

3. 子阶段一：收集特征（collect）
   a. 逐个分析种子接口（恢复时从 seeds.current 指向的种子继续）：
      - 找到该函数在代码中的全部使用点（优先 codegraph_explore，否则 grep/ast-grep）
      - 依次排查每个使用点，判断是否是注册点：
        · 将函数作为参数传递给另一个函数 → 动态注册特征
        · 将函数赋值给某个结构体字段/数组元素 → 静态注册特征
        · 普通的函数调用 → 不是注册点
      - 确认注册点后，继续追踪注册代码的实现：
        · 静态注册：记录包含注册点的全局变量类型和名称
        · 动态注册：记录注册函数的签名和参数位置
      - 将提取的特征加入 patterns 列表
   b. 种子分析完成后，对 patterns 列表进行去重：
      - 去重需要深度理解代码语义。两个种子函数如果绑定到同一个 handler 机制
        的不同实例（如同一个全局数组的不同位置），合并为一个 pattern
      - 去重后为每个 pattern 分配独立编号

4. 子阶段二：推广匹配（match）
   a. 对每个 pattern，在全代码范围内搜索：
      - 静态注册：搜索同类型全局变量/数组的定义，提取其中绑定的函数
      - 动态注册：搜索注册函数的所有调用点，提取被注册的函数
   b. 排除已在 inherited_apis 中的接口
   c. 每个候选接口必须通过 arch.md 的通信边界信息做 LLM 交叉验证：
      - 无法与任何通信边界关联的 → 排除
      - 能与通信边界关联的 → 加入 candidate_apis
   d. 从断点恢复时，从 patterns 中 status=pending 的第一个 pattern 继续

5. 输出 tmp/feature_apis.json
```

### tmp/feature_state.json 结构

```json
{
  "sub_phase": "collect|match",

  "seeds": {
    "total": 12,
    "current": 3,
    "done": [1, 2],
    "remaining": [4, 5, 6, 7, 8, 9, 10, 11, 12]
  },

  "patterns": [
    {
      "id": 1,
      "status": "analyzed",
      "summary": "全局结构体数组 ap_msg_handle_func 的静态绑定，字段 handler 赋值",
      "source_seed": "start_push_proxy",
      "details": {
        "type": "static",
        "var_type": "ap_msg_handle_func",
        "var_name": "push_conn_func_table",
        "binding_field": "handler"
      }
    },
    {
      "id": 2,
      "status": "pending",
      "summary": "注册函数 ai_svc_mail_dispatcher_register_wrapper 的动态绑定，第2个参数为被注册函数",
      "source_seed": "ai_svc_fusion_sets_handler",
      "details": {
        "type": "dynamic",
        "register_func": "ai_svc_mail_dispatcher_register_wrapper",
        "callback_param_index": 2
      }
    }
  ],

  "candidate_apis": [
    {"name": "func_x", "file": "x.c", "matched_pattern_id": 1}
  ]
}
```

**设计要点：**

- seeds 编号为接口在 inherited_apis.json 中的序号（1-based）
- patterns 独立编号，去重后分配。analyzed/pending 用 pattern id 追踪进度
- details 结构可扩展：type 是注册类型标签，details 内部字段随 type 变化。当出现新的注册模式（间接注册、多层包装、宏注册等），新增 type 名称，扩展 details 字段，不破坏已有 pattern 结构。扩展规则：type 必填、summary 必填、source_seed 必填，details 内字段自由但需自描述

---

## 七、架构识别接口（arch_identify）

最核心的复杂阶段。根据 arch.md 的信息，逐文件深入分析，识别外部接口。

**本阶段永不跳过。** 即使无继承接口、无特征匹配，架构识别也必须执行。

### 流程

```
1. 进入条件检查：
   a. 用两种方法检查 tmp/arch_identify_state.json
      ├── 确认存在 → 加载状态，从中断位置恢复
      └── 确认不存在 → 全新启动

2. 全新启动时，准备任务列表：
   a. 读取 arch.md 的"代码分区映射"，提取待分析文件列表
   b. 按优先级排序：核心通信文件在前，内部实现文件在后
   c. 每 10 个文件一批，生成全部 batches 写入 state
   d. 保存 arch_identify_state.json

3. 批次分析循环（从 batch_current 开始）：
   a. 读取当前批次的文件（最多 10 个）
   b. 对每个文件，结合 arch.md 的通信边界信息，寻找两种形式的外部接口：

      形式一：参数传入
      - 函数被注册为 handler/回调，供外部模块调用
      - 函数自身不被本项目代码调用（或仅在注册点被引用）
      - 函数参数来自外部不可信源

      形式二：信道读取
      - 函数体内调用共享内存读取、文件读取、IPC消息接收等操作
      - 从这些信道获取的数据即为外部输入

   c. 对找到的每个候选接口，LLM 深入验证：
      - 函数功能是什么？与哪个外部模块通信（参考 arch.md）？
      - 输入数据的来源是项目内部还是外部？
      - 如果是内部函数间的数据传递 → 排除
      - 无法与任何通信边界关联 → 排除（误报优先原则）
   d. 本批次完成后，更新 state：批次状态 → completed，found_apis 追加

4. 全部批次完成后，输出 tmp/arch_apis.json
```

### 关键规则

- found_apis 不包含已在 inherited_apis.json 或 feature_apis.json 中的接口，避免重复
- 分析必须覆盖 file_list 中全部文件，不因已找到足够接口而提前退出
- 如果某个文件完全不含外部接口，不做记录，继续下一个
- 判断"是否为外部输入"时存疑则排除；仅明确的外部通信入口才保留

### tmp/arch_identify_state.json 结构

```json
{
  "file_list": ["src/comm/a.c", "src/comm/b.c", ...],
  "total_files": 50,
  "batch_size": 10,
  "batches": [
    {"index": 1, "files": ["src/comm/a.c", ...], "status": "completed"},
    {"index": 2, "files": ["src/comm/k.c", ...], "status": "completed"},
    {"index": 3, "files": ["src/ipc/x.c", ...], "status": "in_progress"},
    {"index": 4, "files": ["src/ipc/y.c", ...], "status": "pending"}
  ],
  "batch_current": 3,
  "found_apis": [
    {"name": "func_p", "file": "src/comm/a.c", "form": "parameter_input", "reason": "注册为IPC消息处理器，data参数来自外部发送方"},
    {"name": "func_q", "file": "src/ipc/x.c", "form": "channel_read", "reason": "内部调用 read_from_shared_memory 获取外部模块写入的数据"}
  ]
}
```

---

## 八、外部接口筛选（filter）

合并前三阶段的接口列表，逐条审查，排除不符合条件的接口。

### 流程

```
1. 合并接口列表：
   inherited_apis ∪ feature_apis ∪ arch_apis → 去重后形成 api_pool
   去重依据：函数名 + 文件路径一致

2. 用两种方法检查 black_api.json 是否存在
   └── 确认存在 → 加载黑名单，函数名+文件路径匹配的直接排除，不进入后续审查

3. 全新启动时，写入 filter_state.json
   断点恢复时，从 state_file 加载，跳过 reviewed: true 的项

4. 逐条审查循环：
   a. 读取函数代码（如果之前阶段已充分阅读可跳过）
   b. 按以下规则判断保留/排除：

      规则一：测试函数排除
        建议规则：函数名含 test_/_test/mock_/stub_/demo_/sample_ 前缀或后缀，
        或位于 test/tests/unittest/mock 目录下。
        同时允许 LLM 根据上下文（注释、调用链）判断，即使不完全匹配建议规则
        也可能是测试函数。

      规则二：无外部输入排除（核心规则）
        判断函数是否接收外部输入。
        以下情况排除：
        - 无入参，且函数体内无从共享内存/文件/IPC等信道获取外部输入
        - 有入参，但全部是系统框架类参数（如 struct file *f、struct inode *i、
          内核内部数据结构的指针、框架回调约定的上下文参数等），不是外部用户/模块
          传入的数据
        - 有入参，但参数用于输出目的（如 void *out_buffer、int *result），
          根据代码语义判断
        以下情况保留：
        - 无入参，但函数体内从共享内存/文件/IPC等信道读取了外部数据
        - 有入参，且存在至少一个参数携带外部输入数据

      规则三：冗余函数排除
        建议规则：函数体为空、仅调用另一个同名函数、被注释标记为
        deprecated/unused、纯日志/调试打印函数。
        同时允许 LLM 根据代码语义判断。

   c. 规则判断边界模糊时 → 倾向排除（误报优先原则）
   d. reason 字段必须写明确凿证据，不可用模糊描述

5. 审查完成，从 api_list 提取 decision=keep 的接口 → api.json
```

### tmp/filter_state.json 结构

```json
{
  "api_total": 30,
  "api_list": [
    {"name": "func_a", "file": "a.c", "reviewed": true, "decision": "keep", "reason": "接收外部IPC消息请求，data参数携带外部不可信数据"},
    {"name": "func_b", "file": "b.c", "reviewed": true, "decision": "exclude", "reason": "无入参，函数体内仅为内部状态更新逻辑，无外部数据源"},
    {"name": "func_c", "file": "c.c", "reviewed": false, "decision": null, "reason": null}
  ]
}
```

---

## 九、最终输出

```
1. 输出 api.json（与 old_api.json 格式一致）：
   [
     {"name": "FUNC_NAME", "file": "FILE_PATH"},
     ...
   ]
   name 为函数名，file 为文件绝对路径。按 api_list 中保留接口的顺序排列。

2. 输出 summary.md：
   按 api.json 顺序，逐条说明识别理由。每条包含：
   - 函数名 + 文件路径
   - 识别路径：继承 / 特征识别 / 架构识别（可能有多个路径命中同一接口）
   - 识别理由摘要（该接口与哪个外部模块通信，通过什么信道）
   - 筛选阶段保留原因（接收什么外部输入）

3. 更新 progress.json → phase: "done"
```

---

## 十、附加要求

1. 整个 skill 的提示词用中文。相同语义的用词前后保持一致。
2. 所有分析由主 agent 完成，不使用 sub-agent 并行分析。
3. 需要申请代码仓探索常用工具的权限（Bash、Read、Write、Grep 等）。当 codegraph 可用时优先使用 MCP 工具 codegraph_explore，但如果环境未安装 codegraph 或未配置 MCP，不中断分析，使用常规工具。
4. 适用业务：嵌入式/底层系统代码（Linux 内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU 固件）。
5. SKILL.md 的激活方式：`/api-finder <project_dir>`。
