#include "ipc_handler.h"

// 模拟内核/固件中的 MMIO 读取
#define readl(addr) (*(volatile unsigned int *)(addr))

// [外部接口-channel_read] 从 IPC 消息队列读取外部进程发来的消息
int handle_ipc_queue(void) {
    char buf[256];
    // msgrcv 是 IPC 消息接收的系统调用
    // msgrcv(msgid, buf, sizeof(buf), 0, 0);
    return 0;
}

// [外部接口-channel_read] 从 MMIO 寄存器读取外部硬件状态
unsigned int read_device_status(void) {
    unsigned int status = readl((volatile unsigned int *)0xFE000000);
    return status;
}

// [外部接口] 写入响应到 IPC——这是写操作，无外部输入（但参数来自外部输入路径）
int ipc_write_response(int msg_type, const void *data, int len) {
    // msgsnd 是写操作
    // msgsnd(msgid, data, len, 0);
    return len;
}
