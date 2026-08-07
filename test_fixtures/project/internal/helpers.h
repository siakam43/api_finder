#ifndef HELPERS_H
#define HELPERS_H

int calculate_checksum(const char *data, int len);
int validate_buffer(const void *buf, int size);
void log_message(const char *msg);
int stub_forwarder(void *data, int len);

#endif
