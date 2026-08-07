# api-fixer / api-finder / api-cleaner 系统测试计划

## 测试项目结构

```
test_fixtures/project/
├── core/dispatcher.{c,h}     # handler注册函数（parameter_input 来源）
├── comm/msg_handler.{c,h}    # 迁移后的函数 + IPC数据读取（channel_read）
├── io/ipc_handler.{c,h}      # IPC队列读取 + MMIO寄存器读取
├── internal/helpers.{c,h}    # 内部工具函数（应被排除）
└── tests/test_mocks.c        # 测试/mock函数（应被排除）
```

## api-fixer 测试用例

| # | 场景 | old_api 条目 | 预期结果 |
|---|------|-------------|---------|
| 1 | 路径在范围，函数定义存在 | handle_user_request → core/dispatcher.c | inherited, path unchanged |
| 2 | 路径在范围，函数已迁移 | handler_func_b → core/dispatcher.c (实际在 comm/) | inherited, path_updated=true |
| 3 | 路径在范围，函数已删除 | handler_func_c → core/dispatcher.c (不存在) | eliminated |
| 4 | 路径不在范围（文件不存在），fallback 无结果 | legacy_handler → core/legacy.c | eliminated |
| 5 | 同名去重：第一个继承，第二个淘汰 | duplicate_name ×2 | 1 inherited + 1 eliminated |
| 6 | 绝对路径，正常继承 | process_ipc_message | inherited |
| 7 | 相对路径，正常化后继承 | read_from_shared_memory (相对路径) | inherited |
| 8 | 相对路径，正常化后继承 | handle_ipc_queue (相对路径) | inherited |
| 9 | 文件不存在，fallback 无结果 | deleted_api → core/removed.c | eliminated |

**预期 api-fixer inherited_apis.json: 6 个条目**

## api-finder 测试用例

| # | 场景 | 验证点 |
|---|------|-------|
| 1 | 无 api-fixer 输出时可正常运行 | §4 跳过，进入 §5 |
| 2 | feature 阶段提取注册模式 | 从种子中识别 cmd_table[] 静态注册 |
| 3 | arch_identify - parameter_input | 识别 handle_system_event, dispatch_ioctl |
| 4 | arch_identify - channel_read | 识别 read_device_status |
| 5 | filter - 黑名单排除 | handle_user_request 被 black_api 排除 |
| 6 | filter - 测试函数排除 | test_handler_init, mock_ipc_receive, demo_feature_usage |
| 7 | filter - 无外部输入排除 | calculate_checksum (内部调用) |
| 8 | filter - 冗余函数排除 | log_message (空函数), stub_forwarder (纯转发) |
| 9 | filter - 内部调用者排除 | internal_dispatcher (被 dispatch_ioctl 调用) |
| 10 | 输出 api.json + finder_summary.md | JSON 格式正确，绝对路径 |

## api-cleaner 测试用例

| # | 场景 | 验证点 |
|---|------|-------|
| 1 | 保留 - parameter_input 有外部输入 | dispatch_ioctl (arg 来自用户态) |
| 2 | 保留 - channel_read 有外部输入 | read_device_status (MMIO read) |
| 3 | 保留 - channel_read IPC | handle_ipc_queue (msgrcv) |
| 4 | 排除 - 内部调用者 | 如有函数被 internal_dispatcher 调用 |
| 5 | 排除 - 测试/mock 函数 | 过滤 name/file 匹配 |
| 6 | 排除 - 写操作无外部输入 | ipc_write_response (仅 msgsnd 写) |
| 7 | 排除 - 无入参且无信道读取 | 函数无外部输入来源 |
| 8 | 输出 api_clean.json + cleaner_summary.md | JSON 包含 form/taint_data 字段 |

## 运行方式

```
# 1. api-fixer
/api-fixer test_fixtures/project

# 2. api-finder (依赖 api-fixer 输出)
/api-finder test_fixtures/project

# 3. api-cleaner (依赖 api-finder 输出)
/api-cleaner test_fixtures/project
```
