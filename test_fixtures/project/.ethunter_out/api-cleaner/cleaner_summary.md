# 接口去误报分析报告 — /home/admin/cc/wksp/siakam_security_skills/api_finder/test_fixtures/project

## 分析概要
- 输入接口数: 7
- 保留: 5
- 排除: 2

## 保留接口

### 1. handler_func_b — /home/admin/cc/wksp/siakam_security_skills/api_finder/test_fixtures/project/comm/msg_handler.c
- **接口形式:** parameter_input
- **通信边界:** 固件通信子系统 → 外部进程，通过 IPC 消息队列
- **外部输入:** msg_type（消息类型）与 msg_data（消息数据指针，void*），承载外部进程经 IPC 消息队列发来的消息数据
- **保留原因:** 参数 msg_type/msg_data 均非框架/输出参数（外部输入参数个数=2），无需类型判断直接保留；全项目搜索仅有头文件声明与定义、无内部调用者；msg_type+msg_data 构成 IPC 消息（type+payload）格式，位于 comm/msg_handler.c（arch.md 界定的 IPC 消息处理文件），与"外部进程→IPC 消息队列"边界匹配。该接口为继承接口（api-fixer 已将路径从 core/dispatcher.c 更新为 comm/msg_handler.c）

### 2. process_ipc_message — /home/admin/cc/wksp/siakam_security_skills/api_finder/test_fixtures/project/comm/msg_handler.c
- **接口形式:** parameter_input
- **通信边界:** 固件通信子系统 → 外部进程，通过 IPC 消息队列
- **外部输入:** ipc_buf（IPC 消息缓冲区指针）与 buf_len（缓冲区长度），来自外部进程
- **保留原因:** 注释标记为注册的 IPC 消息处理回调；ipc_buf/buf_len 均非框架/输出参数（外部输入参数个数=2）；全项目搜索无调用点（仅头文件声明与定义）；ipc_buf/buf_len 与 IPC 消息缓冲区格式匹配，与"外部进程→IPC 消息队列"边界匹配

### 3. read_from_shared_memory — /home/admin/cc/wksp/siakam_security_skills/api_finder/test_fixtures/project/comm/msg_handler.c
- **接口形式:** both
- **通信边界:** 固件通信子系统 → 外部模块（共享内存），通过共享内存
- **外部输入:** offset/size 参数指定读取范围；函数体 memcpy 从共享内存地址 0x10000000 读取 size 字节
- **保留原因:** dst 为 memcpy 目标、仅写入不读取，属输出参数跳过；offset/size 非框架/非输出参数（外部输入参数个数=2）；memcpy 从共享内存读取整段缓冲区（非单整数读取）；与"外部模块（共享内存）→共享内存"边界匹配

### 4. handle_ipc_queue — /home/admin/cc/wksp/siakam_security_skills/api_finder/test_fixtures/project/io/ipc_handler.c
- **接口形式:** channel_read
- **通信边界:** 固件通信子系统 → 外部进程，通过 IPC 消息队列
- **外部输入:** msgrcv(1, buf, sizeof(buf)=256, 0, 0) 从 IPC 消息队列接收 256 字节消息到 buf
- **保留原因:** 信道传输量为 256 字节缓冲区而非单个整数（2.5.b 单整数排除不适用）；无入参、全项目无调用者；与"外部进程→IPC 消息队列"边界匹配

### 6. dispatch_ioctl — /home/admin/cc/wksp/siakam_security_skills/api_finder/test_fixtures/project/core/dispatcher.c
- **接口形式:** parameter_input
- **通信边界:** 固件通信子系统 → 用户态应用程序，通过 ioctl 系统调用
- **外部输入:** request（请求码）与 arg（void* 数据指针），arg 承载用户态经 copy_from_user 传入的数据
- **保留原因:** ioctl dispatcher（注释与 finder_summary 架构识别一致）；request/arg 均非框架/输出参数（外部输入参数个数=2）；cmd_table[] 中 {3, dispatch_ioctl} 为注册点而非调用、全项目无调用者；与"用户态应用程序→ioctl 系统调用"边界匹配

## 排除接口

### 5. handle_system_event — /home/admin/cc/wksp/siakam_security_skills/api_finder/test_fixtures/project/core/dispatcher.c
- **排除原因:** 无法与 arch.md 通信边界表中任一边界建立明确关联：finder_summary 声称关联的"外部事件系统"不在边界表（用户态应用程序/外部进程/外部模块（共享内存）/硬件设备）中；cmd_table[] 与 event_handlers[] 两个注册点均无法对应表中任一条边界，按 2.4.d"无法与任何通信边界明确关联→排除"处理

### 7. read_device_status — /home/admin/cc/wksp/siakam_security_skills/api_finder/test_fixtures/project/io/ipc_handler.c
- **排除原因:** readl((volatile unsigned int *)0xFE000000) 读回的原始数据仅为单个 uint32 状态寄存器值（非内存地址、未用于内存操作），属单整数信道读取，按 2.5.b 排除
