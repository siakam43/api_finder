#ifndef MSG_HANDLER_H
#define MSG_HANDLER_H

int process_ipc_message(void *ipc_buf, int buf_len);
int read_from_shared_memory(void *dst, int offset, int size);
int handler_func_b(int msg_type, void *msg_data);
int duplicate_name(int mode);

#endif
