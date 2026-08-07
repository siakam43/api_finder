#ifndef DISPATCHER_H
#define DISPATCHER_H

typedef struct {
    int cmd;
    int param_count;
} cmd_context;

int handle_user_request(int cmd, void *data, int len);
int handle_system_event(void *event_data);
int dispatch_ioctl(unsigned int request, void *arg);
int internal_dispatcher(void *data, int len);

#endif
