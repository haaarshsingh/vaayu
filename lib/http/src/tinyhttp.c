#include "tinyhttp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>

#define MAX_REQUEST_LINE 8192
#define MAX_PATH_LEN 4096
#define RECV_TIMEOUT_SEC 5

typedef struct {
    const char *ext;
    const char *mime;
} mime_entry;

static const mime_entry mime_map[] = {
    {"html", "text/html; charset=utf-8"},
    {"htm", "text/html; charset=utf-8"},
    {"css", "text/css"},
    {"js", "application/javascript"},
    {"json", "application/json"},
    {"txt", "text/plain; charset=utf-8"},
    {"png", "image/png"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"gif", "image/gif"},
    {"svg", "image/svg+xml"},
    {"pdf", "application/pdf"},
    {"ico", "image/x-icon"},
    {"wasm", "application/wasm"},
    {NULL, NULL}
};

static const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    
    ext++; // Skip the dot
    for (const mime_entry *entry = mime_map; entry->ext; entry++) {
        if (strcasecmp(ext, entry->ext) == 0) {
            return entry->mime;
        }
    }
    return "application/octet-stream";
}

static void format_http_date(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    strftime(buf, len, "%a, %d %b %Y %H:%M:%S GMT", gmt);
}

static int send_error(int fd, int status, const char *reason, int is_head) {
    char date_buf[64];
    format_http_date(date_buf, sizeof(date_buf));
    
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

static int normalize_path(const char *request_path, char *normalized) {
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

int th_init(th_server *srv, const char *docroot, int port, const char *index_file) {
    char resolved_docroot[PATH_MAX];
    if (!realpath(docroot, resolved_docroot)) {
        return errno;
    }
    
    srv->docroot = strdup(resolved_docroot);
    srv->index_file = strdup(index_file ? index_file : "index.html");
    srv->port = port;
    srv->max_header_bytes = 16384;
    
    srv->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->listen_fd < 0) return errno;
    
    int opt = 1;
    if (setsockopt(srv->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(srv->listen_fd);
        return errno;
    }
    
    int flags = fcntl(srv->listen_fd, F_GETFD);
    if (flags >= 0) {
        fcntl(srv->listen_fd, F_SETFD, flags | FD_CLOEXEC);
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(srv->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(srv->listen_fd);
        return errno;
    }
    
    if (listen(srv->listen_fd, 1) < 0) {
        close(srv->listen_fd);
        return errno;
    }
    
    return 0;
}

void th_close(th_server *srv) {
    if (srv->listen_fd >= 0) {
        close(srv->listen_fd);
        srv->listen_fd = -1;
    }
    free((void*)srv->docroot);
    free((void*)srv->index_file);
    srv->docroot = NULL;
    srv->index_file = NULL;
}

int th_serve_once(th_server *srv) {
    int client_fd = accept(srv->listen_fd, NULL, NULL);
    if (client_fd < 0) return errno;
    
    struct timeval timeout = {RECV_TIMEOUT_SEC, 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    char buffer[MAX_REQUEST_LINE];
    char *buf_pos = buffer;
    size_t buf_remain = sizeof(buffer) - 1;
    size_t total_headers = 0;
    int request_line_done = 0;
    int headers_done = 0;
    
    while (!headers_done && total_headers < srv->max_header_bytes) {
        ssize_t n = recv(client_fd, buf_pos, buf_remain, 0);
        if (n <= 0) {
            close(client_fd);
            return n < 0 ? errno : ECONNRESET;
        }
        
        buf_pos[n] = '\0';
        total_headers += n;
        
        if (!request_line_done) {
            char *line_end = strstr(buffer, "\r\n");
            if (line_end) {
                *line_end = '\0';
                request_line_done = 1;
                
                if (strstr(line_end + 2, "\r\n\r\n")) {
                    headers_done = 1;
                }
            }
        } else {
            if (strstr(buffer, "\r\n\r\n")) {
                headers_done = 1;
            }
        }
        
        buf_pos += n;
        buf_remain -= n;
        if (buf_remain == 0) break;
    }
    
    if (!request_line_done || total_headers >= srv->max_header_bytes) {
        send_error(client_fd, 400, "Bad Request", 0);
        close(client_fd);
        return 0;
    }
    
    char method[16], path[MAX_PATH_LEN], version[16];
    if (sscanf(buffer, "%15s %4095s %15s", method, path, version) != 3) {
        send_error(client_fd, 400, "Bad Request", 0);
        close(client_fd);
        return 0;
    }
    
    int is_head = 0;
    if (strcmp(method, "GET") == 0) {
        is_head = 0;
    } else if (strcmp(method, "HEAD") == 0) {
        is_head = 1;
    } else {
        send_error(client_fd, 405, "Method Not Allowed", 0);
        close(client_fd);
        return 0;
    }
    
    char normalized_path[MAX_PATH_LEN];
    if (normalize_path(path, normalized_path) < 0) {
        send_error(client_fd, 404, "Not Found", is_head);
        close(client_fd);
        return 0;
    }
    
    char file_path[PATH_MAX];
    int ret = snprintf(file_path, sizeof(file_path), "%s%s", srv->docroot, normalized_path);
    if (ret >= sizeof(file_path)) {
        send_error(client_fd, 404, "Not Found", is_head);
        close(client_fd);
        return 0;
    }
    
    char resolved_path[PATH_MAX];
    if (!realpath(file_path, resolved_path)) {
        send_error(client_fd, 404, "Not Found", is_head);
        close(client_fd);
        return 0;
    }
    
    if (strncmp(resolved_path, srv->docroot, strlen(srv->docroot)) != 0) {
        send_error(client_fd, 404, "Not Found", is_head);
        close(client_fd);
        return 0;
    }
    
    struct stat st;
    if (stat(resolved_path, &st) < 0) {
        send_error(client_fd, 404, "Not Found", is_head);
        close(client_fd);
        return 0;
    }
    
    if (S_ISDIR(st.st_mode)) {
        ret = snprintf(file_path, sizeof(file_path), "%s/%s", resolved_path, srv->index_file);
        if (ret >= sizeof(file_path) || !realpath(file_path, resolved_path)) {
            send_error(client_fd, 404, "Not Found", is_head);
            close(client_fd);
            return 0;
        }
        
        if (stat(resolved_path, &st) < 0) {
            send_error(client_fd, 404, "Not Found", is_head);
            close(client_fd);
            return 0;
        }
    }
    
    if (!S_ISREG(st.st_mode)) {
        send_error(client_fd, 404, "Not Found", is_head);
        close(client_fd);
        return 0;
    }
    
    int file_fd = open(resolved_path, O_RDONLY);
    if (file_fd < 0) {
        send_error(client_fd, 500, "Internal Server Error", is_head);
        close(client_fd);
        return 0;
    }
    
    char date_buf[64];
    format_http_date(date_buf, sizeof(date_buf));
    
    const char *mime_type = get_mime_type(resolved_path);
    
    char headers[1024];
    int headers_len = snprintf(headers, sizeof(headers),
        "HTTP/1.0 200 OK\r\n"
        "Date: %s\r\n"
        "Server: tinyc/0.1\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        date_buf, mime_type, (long long)st.st_size);
    
    send(client_fd, headers, headers_len, 0);
    
    if (!is_head) {
        char file_buf[8192];
        ssize_t bytes_read;
        while ((bytes_read = read(file_fd, file_buf, sizeof(file_buf))) > 0) {
            send(client_fd, file_buf, bytes_read, 0);
        }
    }
    
    close(file_fd);
    close(client_fd);
    return 0;
}
