#ifndef NET_H
#define NET_H

#include <3ds.h>

int net_init(void);
void net_exit(void);

int http_health_check(const char* host, int port);
int http_post(const char* url, const char* post_data, char* response, int resp_size);
int http_post_audio(const char* url, const u8* audio_data, int audio_len,
                    char* response, int resp_size);

#endif // NET_H
