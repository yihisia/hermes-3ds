/**
 * Networking module for Hermes 3DS Client
 * Uses libctru SOC service + raw HTTP (no libcurl to keep binary small)
 */

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include "net.h"

#define SOC_ALIGN 0x1000
#define SOC_BUFFERSIZE 0x100000

static u32* soc_buffer = NULL;

int net_init(void) {
    soc_buffer = memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (!soc_buffer) return -1;
    
    Result ret = socInit(soc_buffer, SOC_BUFFERSIZE);
    if (R_FAILED(ret)) {
        free(soc_buffer);
        soc_buffer = NULL;
        return -2;
    }
    
    return 0;
}

void net_exit(void) {
    socExit();
    if (soc_buffer) {
        free(soc_buffer);
        soc_buffer = NULL;
    }
}

static int connect_to_server(const char* host, int port) {
    struct sockaddr_in addr;
    int sock;
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    // Set timeout (10 seconds)
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -2;
    }
    
    return sock;
}

int http_health_check(const char* host, int port) {
    int sock = connect_to_server(host, port);
    if (sock < 0) return sock;
    
    char request[256];
    snprintf(request, sizeof(request),
        "GET /health HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n\r\n",
        host, port);
    
    send(sock, request, strlen(request), 0);
    
    char buf[512];
    int received = recv(sock, buf, sizeof(buf) - 1, 0);
    close(sock);
    
    if (received <= 0) return -3;
    buf[received] = '\0';
    
    // Check for 200 OK
    if (strstr(buf, "200 OK")) return 0;
    return -4;
}

// URL encode a string
static void url_encode(const char* src, char* dst, int dst_size) {
    static const char hex[] = "0123456789ABCDEF";
    int di = 0;
    
    for (int i = 0; src[i] && di < dst_size - 4; i++) {
        char c = src[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
            dst[di++] = c;
        } else {
            dst[di++] = '%';
            dst[di++] = hex[(c >> 4) & 0xF];
            dst[di++] = hex[c & 0xF];
        }
    }
    dst[di] = '\0';
}

int http_post(const char* url, const char* post_data, char* response, int resp_size) {
    // Parse host and port from URL
    char host[128];
    int port = 80;
    char path[256] = "/";
    
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    
    // Extract host
    int hi = 0;
    while (*p && *p != ':' && *p != '/' && hi < (int)sizeof(host) - 1) {
        host[hi++] = *p++;
    }
    host[hi] = '\0';
    
    // Extract port
    if (*p == ':') {
        p++;
        port = atoi(p);
        while (*p && *p != '/') p++;
    }
    
    // Extract path
    if (*p == '/') {
        strncpy(path, p, sizeof(path) - 1);
    }
    
    int sock = connect_to_server(host, port);
    if (sock < 0) return sock;
    
    // URL encode the post data
    char encoded_data[1024];
    // We need to encode the message value part
    // post_data format: "message=xxx&history=true"
    // Find the message value and encode it
    char encoded_post[1024];
    const char* msg_start = strstr(post_data, "message=");
    if (msg_start) {
        msg_start += 8;
        const char* amp = strstr(msg_start, "&");
        char msg_value[512];
        if (amp) {
            int len = amp - msg_start;
            strncpy(msg_value, msg_start, len);
            msg_value[len] = '\0';
        } else {
            strncpy(msg_value, msg_start, sizeof(msg_value) - 1);
        }
        
        char encoded_msg[768];
        url_encode(msg_value, encoded_msg, sizeof(encoded_msg));
        snprintf(encoded_post, sizeof(encoded_post), "message=%s&history=true", encoded_msg);
    } else {
        strncpy(encoded_post, post_data, sizeof(encoded_post) - 1);
    }
    
    // Build HTTP request
    char request[2048];
    int req_len = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n"
        "%s",
        path, host, port,
        (int)strlen(encoded_post),
        encoded_post);
    
    // Send request
    int sent = send(sock, request, req_len, 0);
    if (sent < 0) {
        close(sock);
        return -10;
    }
    
    // Receive response
    int total = 0;
    while (total < resp_size - 1) {
        int n = recv(sock, response + total, resp_size - 1 - total, 0);
        if (n <= 0) break;
        total += n;
    }
    response[total] = '\0';
    
    close(sock);
    
    // Find body (after \r\n\r\n)
    char* body = strstr(response, "\r\n\r\n");
    if (body) {
        body += 4;
        memmove(response, body, strlen(body) + 1);
    }
    
    return 0;
}

int http_post_audio(const char* url, const u8* audio_data, int audio_len,
                    char* response, int resp_size) {
    // Parse host/port from URL
    char host[128];
    int port = 80;
    char path[256] = "/";
    
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    
    int hi = 0;
    while (*p && *p != ':' && *p != '/' && hi < (int)sizeof(host) - 1) {
        host[hi++] = *p++;
    }
    host[hi] = '\0';
    
    if (*p == ':') {
        p++;
        port = atoi(p);
        while (*p && *p != '/') p++;
    }
    if (*p == '/') {
        strncpy(path, p, sizeof(path) - 1);
    }
    
    int sock = connect_to_server(host, port);
    if (sock < 0) return sock;
    
    // Build multipart form data
    const char* boundary = "----3dsHermesBoundary";
    
    char header[1024];
    int hdr_len = snprintf(header, sizeof(header),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: multipart/form-data; boundary=%s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"audio\"; filename=\"voice.raw\"\r\n"
        "Content-Type: audio/pcm\r\n\r\n",
        path, host, port, boundary,
        audio_len + strlen(boundary) * 2 + 100,
        boundary);
    
    char footer[128];
    int foot_len = snprintf(footer, sizeof(footer), "\r\n--%s--\r\n", boundary);
    
    // Send header
    send(sock, header, hdr_len, 0);
    
    // Send audio data in chunks
    int offset = 0;
    while (offset < audio_len) {
        int chunk = audio_len - offset;
        if (chunk > 4096) chunk = 4096;
        int sent = send(sock, audio_data + offset, chunk, 0);
        if (sent <= 0) break;
        offset += sent;
    }
    
    // Send footer
    send(sock, footer, foot_len, 0);
    
    // Receive response
    int total = 0;
    while (total < resp_size - 1) {
        int n = recv(sock, response + total, resp_size - 1 - total, 0);
        if (n <= 0) break;
        total += n;
    }
    response[total] = '\0';
    
    close(sock);
    
    // Find body
    char* body = strstr(response, "\r\n\r\n");
    if (body) {
        body += 4;
        memmove(response, body, strlen(body) + 1);
    }
    
    return 0;
}
