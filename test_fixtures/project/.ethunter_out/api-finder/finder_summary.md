# 外部接口识别报告 — test_fixtures

## 1. handler_func_b — comm/msg_handler.c
- **识别路径：** 继承（从 core/dispatcher.c 迁移，路径已更新）
- **通信边界：** 外部进程 → IPC 消息队列
- **外部输入：** msg_data 指针承载 IPC 消息数据
- **保留原因：** 历史继承接口，无内部调用者，参数 void* msg_data 携带外部 IPC 数据

## 2. process_ipc_message — comm/msg_handler.c
- **识别路径：** 继承
- **通信边界：** 外部进程 → IPC 消息队列
- **外部输入：** ipc_buf 指针承载外部 IPC 进程发来的数据
- **保留原因：** 历史继承接口，注册为 IPC 消息处理回调，ipc_buf 接收外部数据

## 3. read_from_shared_memory — comm/msg_handler.c
- **识别路径：** 继承
- **通信边界：** 外部模块 → 共享内存
- **外部输入：** 从共享内存地址 0x10000000 通过 memcpy 读取外部模块写入的数据
- **保留原因：** 历史继承接口，channel_read 形式，从共享内存读取外部数据

## 4. handle_ipc_queue — io/ipc_handler.c
- **识别路径：** 继承
- **通信边界：** 外部进程 → IPC 消息队列
- **外部输入：** 通过 msgrcv 从 IPC 队列接收外部进程消息到 buf
- **保留原因：** 历史继承接口，channel_read 形式，从 IPC 消息队列读取外部数据

## 5. handle_system_event — core/dispatcher.c
- **识别路径：** 特征识别 + 架构识别
- **通信边界：** 外部事件系统 → 回调
- **外部输入：** void* event_data 指针承载外部事件数据
- **保留原因：** 注册于 cmd_table[] 和 event_handlers[]，无内部调用者，与事件系统通信边界明确关联

## 6. dispatch_ioctl — core/dispatcher.c
- **识别路径：** 架构识别
- **通信边界：** 用户态应用程序 → ioctl
- **外部输入：** void* arg 来自用户态 copy_from_user 填充的缓冲区
- **保留原因：** ioctl dispatcher，注册于 cmd_table[]，arg 承载用户态传入的外部数据，无内部调用者

## 7. read_device_status — io/ipc_handler.c
- **识别路径：** 架构识别
- **通信边界：** 硬件设备 → MMIO 寄存器
- **外部输入：** 通过 readl() 从 MMIO 地址 0xFE000000 读取硬件状态寄存器值
- **保留原因：** channel_read 形式，从 MMIO 寄存器读取外部硬件状态数据
