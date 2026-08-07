#include "msg_handler.h"
#include <string.h>

// [外部接口] 注册为 IPC 消息处理回调
int process_ipc_message(void *ipc_buf, int buf_len) {
    if (!ipc_buf || buf_len <= 0) return -1;
    return 0;
}

// [外部接口] 从共享内存读取外部模块写入的数据
int read_from_shared_memory(void *dst, int offset, int size) {
    void *shm_ptr = (void *)0x10000000;
    memcpy(dst, (char*)shm_ptr + offset, size);
    return size;
}

// 从 core/dispatcher.c 迁移至此的函数
int handler_func_b(int msg_type, void *msg_data) {
    if (msg_type == 0) return -1;
    return 0;
}

// duplicate_name 的第二个定义（与 core/dispatcher.c 冲突——实际编译不会同时存在，但用于测试同名去重逻辑）
int duplicate_name(int mode) {
    return mode + 1;
}
