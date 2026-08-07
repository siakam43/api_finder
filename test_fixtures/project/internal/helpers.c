#include "helpers.h"

// [内部工具函数] 被多处内部代码调用，无外部输入
int calculate_checksum(const char *data, int len) {
    int sum = 0;
    for (int i = 0; i < len; i++) sum += data[i];
    return sum;
}

// [内部工具函数] 参数 buf 来自内部函数传递，不是外部输入
int validate_buffer(const void *buf, int size) {
    if (!buf || size <= 0) return -1;
    return 0;
}

// [空函数/冗余] 仅打印日志，无实际数据处理
void log_message(const char *msg) {
    // LOG(msg);
}

// [纯转发/冗余] 仅调用相同签名的函数
int stub_forwarder(void *data, int len) {
    return validate_buffer(data, len);
}
