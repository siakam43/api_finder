---
name: api-cleaner
description: Use when the user invokes /api-cleaner or asks to remove false positives, clean up, or verify external API interfaces identified by api-finder in embedded/low-level C codebases (Linux kernel drivers, UEFI/BL31/BL2/XLoader firmware, ISP/SensorHub/GPU co-processor firmware). Applies stricter exclusion rules to filter out internal implementation details misidentified as external interfaces.
---

# api-cleaner

消费 api-finder 的输出结果，对每个识别出的外部接口独立重新分析源码，应用更严格的排除规则，去除误报。适用业务场景：嵌入式/底层系统代码（Linux 内核驱动、UEFI/BL31/BL2/XLoader、ISP/SensorHub/GPU 固件）。

## 核心原则

1. **分析质量优先于分析效率。** 宁可慢，不可草率。每个接口的源码必须认真阅读和理解。

2. **更少的误报优先于更少的漏报。** 不确定时倾向于排除。只在有充分证据（代码模式 + 架构信息双重支撑）时才保留接口。

## 使用方法

```
/api-cleaner <project_dir>
```

如果未指定 `project_dir`，默认为当前工作目录。

## 输出

分析完成后在 `<project_dir>/.ethunter_out/api-cleaner/` 下生成：
- `api-clean.json` — 去误报后的外部接口列表
- `summary.md` — 逐接口保留/排除理由说明
- `progress.json` — 断点续分析状态
- `tmp/analysis_state.json` — 逐接口分析中间状态
