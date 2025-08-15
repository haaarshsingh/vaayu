#define _GNU_SOURCE
#include "vaayu_http.h"
#include <sys/socket.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define MAX_PATH_LEN 4096

void vh_format_http_date(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    strftime(buf, len, "%a, %d %b %Y %H:%M:%S GMT", gmt);
}

int vh_send_error(int fd, int status, const char *reason, int is_head) {
    char date_buf[64];
    vh_format_http_date(date_buf, sizeof(date_buf));
    
    const char *body = "";
    char html_body[512];
    if (!is_head) {
        snprintf(html_body, sizeof(html_body),
                 "<!DOCTYPE html><html><head><title>%d %s</title></head>"
                 "<body><h1>%d %s</h1></body></html>",
                 status, reason, status, reason);
        body = html_body;
    }
    
    char headers[1024];
    int headers_len = snprintf(headers, sizeof(headers),
        "HTTP/1.0 %d %s\r\n"
        "Date: %s\r\n"
        "Server: tinyc/0.1\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, reason, date_buf, strlen(body));
    
    if (status == 405) {
        char *allow_pos = strstr(headers, "Connection:");
        if (allow_pos) {
            memmove(allow_pos + 19, allow_pos, strlen(allow_pos) + 1);
            memcpy(allow_pos, "Allow: GET, HEAD\r\n", 18);
            headers_len += 18;
        }
    }
    
    send(fd, headers, headers_len, 0);
    if (!is_head) {
        send(fd, body, strlen(body), 0);
    }
    return 0;
}

int vh_normalize_path(const char *request_path, char *normalized) {
    char *query = strchr(request_path, '?');
    size_t path_len = query ? (size_t)(query - request_path) : strlen(request_path);
    
    if (strchr(request_path, '#')) return -1;
    
    if (strstr(request_path, "..")) return -1;
    
    if (path_len >= MAX_PATH_LEN) return -1;
    memcpy(normalized, request_path, path_len);
    normalized[path_len] = '\0';
    
    char *src = normalized, *dst = normalized;
    while (*src) {
        *dst = *src;
        if (*src == '/' && *(src + 1) == '/') {
            src++;
        } else {
            src++;
            dst++;
        }
    }
    *dst = '\0';
    
    return 0;
}
