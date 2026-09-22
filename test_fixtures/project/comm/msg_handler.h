#ifndef MSG_HANDLER_H
#define MSG_HANDLER_H

int process_ipc_message(void *ipc_buf, int buf_len);
int read_from_shared_memory(void *dst, int offset, int size);
int handler_func_b(int msg_type, void *msg_data);
// 此声明保留但本文件不定义 duplicate_name——定义在 core/dispatcher.c。
// 用于覆盖"old_api 指向的文件在 scope 内、但已不再定义该函数"的同名去重场景（api-fixer 记为 [d1]）。
int duplicate_name(int mode);

#endif
