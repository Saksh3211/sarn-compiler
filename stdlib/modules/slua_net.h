#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int32_t slua_net_init(void);
int64_t slua_net_connect(const char* host, int32_t port);
int64_t slua_net_listen(int32_t port);
int64_t slua_net_accept(int64_t server_id);
int32_t slua_net_send(int64_t id, const char* data);
char*   slua_net_recv(int64_t id, int32_t maxlen);
int32_t slua_net_close(int64_t id);
char*   slua_net_local_ip(void);
int32_t slua_net_send_bytes(int64_t id, const char* data, int32_t len);
#ifdef __cplusplus
}
#endif
