#include "dispatcher.h"

// [外部接口] 作为 ioctl handler 注册，data 来自用户态 copy_from_user
int handle_user_request(int cmd, void *data, int len) {
    if (cmd <= 0) return -1;
    return 0;
}

// [外部接口] 注册为系统事件回调，event_data 来自外部事件系统
int handle_system_event(void *event_data) {
    if (!event_data) return -1;
    return 0;
}

// [外部接口] ioctl dispatcher，arg 来自用户态
int dispatch_ioctl(unsigned int request, void *arg) {
    switch (request) {
        case 1: return handle_user_request(1, arg, 0);
        default: return -1;
    }
}

// [内部函数] 被 dispatch_ioctl 内部调用
int internal_dispatcher(void *data, int len) {
    return 0;
}

// 注册表：将上述 handler 注册为回调
static struct {
    int cmd_id;
    int (*handler)(int, void*, int);
} cmd_table[] = {
    {1, handle_user_request},
    {2, handle_system_event},
    {3, dispatch_ioctl},
};

static struct {
    void *ctx;
    int (*cb)(void*);
} event_handlers[] = {
    {(void*)0x1000, handle_system_event},
};

// duplicate_name 在此定义（第一个）
int duplicate_name(int mode) {
    return mode;
}
