# api-finder 函数定义搜索方法优化设计

## 问题

SKILL.md 中搜索函数定义的方法分散在 4 处（Section 4 Step 2c、Step 2d、Section 5 match、Section 7 filter），
且每处都描述了同一模式：`grep` 匹配 `函数名(` 后跟 `{`。这个模式在 `{` 换行（Allman 风格）时失效，导致遗漏函数定义。

## 方案

将搜索方法统一为两步法：

1. **grep 定位：** `grep -rn "<函数名>" <目标路径>` 获取所有匹配行及行号
2. **Read 验证：** Read 匹配行前后各 3 行上下文，LLM 判断：完整函数签名 + `{`（允许换行）+ 不以 `;` 结尾 + 不在注释中

## 改动

### Section 4（集中定义）

- **新增：** "流程"之前插入 "搜索函数定义的方法" 子章节，定义标准两步法
- **简化 Step 2c：** 原 4 行 grep + filter 描述替换为引用标准方法
- **简化 Step 2d：** 原 3 行 grep 描述替换为引用标准方法

### Section 5 match 子阶段

- 将 `file` 字段说明中的内联 grep 描述替换为独立的两步法描述（grep + Read + 4 条件）

### Section 7 filter 校验

- 函数定义验证步骤：单文件搜索和 scope_files fallback 搜索均替换为两步法描述
