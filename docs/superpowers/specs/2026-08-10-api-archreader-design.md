# api-archreader 设计与 api-finder 适配

## 目标

从 api-finder 中抽离"了解项目架构"（Section 3, arch_analysis）为独立 skill：`api-archreader`，使其可独立运行、输出标准化的 `arch.md`。然后修改 api-finder 消费 api-archreader 的输出，删除自身的架构分析阶段。

## api-archreader skill

### 定位

纯 markdown skill（单文件 `api-archreader/SKILL.md`），只做一件事：分析嵌入式/底层 C 项目的架构，输出 `arch.md`。

### 输入/输出

- **输入**：`/api-archreader <project_dir>`（未指定则用当前工作目录）
- **输出**：`<project_dir>/.ethunter_out/api-archreader/arch.md`

### 架构分析内容

与 api-finder 第三节完全一致：

1. **浏览关键文件** — 从头文件开始，再到核心 .c 文件
2. **三维分析** — 模块概要、外部通信边界（含通信方向/信道）、代码分区映射
3. **生成任务文件列表** — 从代码分区映射整理待分析文件列表，按优先级排序
4. **输出 arch.md** — 模板与 api-finder 第三节完全一致

### 初始化逻辑

直接复用 api-finder 第一节的 scope 确定方式：

- **Step 1**：解析 project_dir
- **Step 2**：确定代码分析范围（clean_code.txt / .etignore / 全量 find）
- **Step 3**：检测 CodeGraph 可用性（可选，不可用则回退到常规工具）
- **Step 4**：初始化输出目录 `mkdir -p <project_dir>/.ethunter_out/api-archreader`

### 恢复机制

极简方案。进入架构分析前：

- 用双方法检查 `arch.md` 是否存在
- 存在 → 跳过，直接告知用户
- 不存在 → 执行架构分析

**无 progress.json，无中间状态文件。**

### 结构（章节规划）

| 章节 | 内容 |
|------|------|
| 核心原则 | 分析质量优先、适用于嵌入式/底层系统 |
| 使用方法 | `/api-archreader <project_dir>` |
| 一、初始化与分析范围确定 | scope 确定（情况A/B/C）+ codegraph 检测 + 输出目录创建 |
| 二、了解项目架构 | 进入检查（arch.md 存在则跳过）→ 浏览关键文件 → 三维分析 → 代码分区映射 → 输出 arch.md |
| 三、权限申请 | Bash / Read / Write / codegraph(可选) / ast-grep(可选) |
| 四、约束规则 | 主 agent 执行、中文、质量优先、文件存在性双方法检查 |
| 五、抗理性化检查 | 常见偷懒想法警示 |
| 六、使用示例 | 首次分析、重复运行的跳过行为 |

### 约束规则

1. 所有分析由主 agent 完成，不使用 sub-agent 并行分析
2. 分析质量优先于分析效率
3. 整个 skill 提示词用中文，相同语义用词前后一致
4. 文件存在性检查严格遵循双方法+判断规则
5. 适用业务：嵌入式/底层系统代码

---

## api-finder 适配

### 变更点

**1. 删除第三节（arch_analysis）全文**

包括进入检查、分析执行（5 个步骤）、arch.md 模板。

**2. 新增前置依赖检查**

在第二节（任务恢复机制）的入口恢复流程中，于步骤 1 和步骤 2 之间新增步骤 1.5：

```
1.5 用两种方法检查 <project_dir>/.ethunter_out/api-archreader/arch.md 是否存在。
     确认不存在 → 报错退出，提示用户先运行 /api-archreader <project_dir>
     确认存在 → 继续步骤 2（正常恢复流程）
```

注意：无论是全新分析还是断点恢复，都必须经过此检查。arch.md 是 api-finder 运行的必要前置条件。

**3. 全局路径替换**

所有引用 `<project_dir>/.ethunter_out/api-finder/arch.md` 的地方改为 `<project_dir>/.ethunter_out/api-archreader/arch.md`：

- 第四节（feature）的前置条件不读 arch.md，不需要改
- 第五节（arch_identify）的"全新启动 — 准备任务列表"步骤 1：`读取 arch.md` → 路径改为 api-archreader 的
- 第六节（filter）的通信边界关联验证引用的 arch.md → 路径改为 api-archreader 的

**4. 章节重编号**

| 原编号 | 新编号 | 内容 |
|--------|--------|------|
| 一 | 一 | 初始化与分析范围确定 |
| 二 | 二 | 任务恢复机制 |
| 三 | （删除） | ~~了解项目架构~~ → 替换为 arch.md 依赖检查 |
| 四 | 三 | 接口特征提取（feature） |
| 五 | 四 | 架构识别接口（arch_identify） |
| 六 | 五 | 外部接口筛选（filter） |
| 七 | 六 | 权限申请 |
| 八 | 七 | 约束规则 |
| 九 | 八 | 使用示例 |

**5. 更新使用示例**

示例中加入 api-archreader 调用步骤。

### 不影响的部分

- feature / arch_identify / filter 的核心分析逻辑
- 断点恢复机制的整体框架（仅入口描述和向后兼容处理有调整）
- api.json / finder_summary.md 的输出格式

### progress.json 处理

删除第三节后，phase 枚举从 `"arch_analysis|feature|arch_identify|filter|done"` 改为 `"feature|arch_identify|filter|done"`。同时移除 `arch_analysis` 状态字段。

**向后兼容：** 断点恢复时遇到旧版 progress.json（phase = `"arch_analysis"`），自动将 phase 更新为 `"feature"`，移除 `arch_analysis` 字段，保存后再继续。

**全新分析入口调整：** 恢复检查中"全新分析，从'三、了解项目架构'开始"改为"全新分析，先检查 api-archreader/arch.md 是否存在，确认存在后从'三、接口特征提取'开始"。

---

## 实施顺序

1. 创建 `api-archreader/SKILL.md`
2. 修改 `api-finder/SKILL.md` 适配新 skill
3. 验证两个 skill 的风格一致性
