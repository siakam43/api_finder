# 接口去误报分析报告 — test_fixtures

## 分析概要
- 输入接口数: 7
- 保留: 7
- 排除: 0

## 保留接口

### 1. handler_func_b — comm/msg_handler.c
- **接口形式:** parameter_input
- **通信边界:** 外部进程 → IPC 消息队列
- **外部输入:** msg_data 指针承载 IPC 消息数据
- **保留原因:** 历史继承接口，msg_data 参数为外部输入指针，无内部调用者，与 IPC 通信边界明确关联

### 2. process_ipc_message — comm/msg_handler.c
- **接口形式:** parameter_input
- **通信边界:** 外部进程 → IPC 消息队列
- **外部输入:** ipc_buf 指针来自外部 IPC 进程数据
- **保留原因:** 注册为 IPC 消息处理回调，ipc_buf 接收外部数据，无内部调用者

### 3. read_from_shared_memory — comm/msg_handler.c
- **接口形式:** channel_read
- **通信边界:** 外部模块 → 共享内存
- **外部输入:** 从共享内存 0x10000000 通过 memcpy 读取外部数据
- **保留原因:** channel_read 形式，从共享内存读取外部模块写入的数据

### 4. handle_ipc_queue — io/ipc_handler.c
- **接口形式:** channel_read
- **通信边界:** 外部进程 → IPC 消息队列
- **外部输入:** 通过 msgrcv 从 IPC 队列接收外部消息到 buf
- **保留原因:** channel_read 形式，msgrcv 从 IPC 队列读取外部进程数据

### 5. handle_system_event — core/dispatcher.c
- **接口形式:** parameter_input
- **通信边界:** 外部事件系统 → 回调
- **外部输入:** event_data 指针承载外部事件数据
- **保留原因:** 注册于 cmd_table[] 和 event_handlers[]，event_data 为外部输入，无内部调用者

### 6. dispatch_ioctl — core/dispatcher.c
- **接口形式:** parameter_input
- **通信边界:** 用户态应用程序 → ioctl
- **外部输入:** arg 指针来自用户态 copy_from_user 填充
- **保留原因:** ioctl dispatcher，arg 承载用户态外部数据，无内部调用者（仅被注册引用）

### 7. read_device_status — io/ipc_handler.c
- **接口形式:** channel_read
- **通信边界:** 硬件设备 → MMIO 寄存器
- **外部输入:** readl(0xFE000000) 读取硬件状态寄存器
- **保留原因:** channel_read 形式，MMIO readl 从硬件寄存器读取外部数据
