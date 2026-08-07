#ifndef IPC_HANDLER_H
#define IPC_HANDLER_H

int handle_ipc_queue(void);
unsigned int read_device_status(void);
int ipc_write_response(int msg_type, const void *data, int len);

#endif
