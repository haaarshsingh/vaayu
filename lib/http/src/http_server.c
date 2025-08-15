#define _GNU_SOURCE
#include "vaayu_http.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#define MAX_REQUEST_LINE 8192
#define MAX_PATH_LEN 4096
#define RECV_TIMEOUT_SEC 5

int vh_init(vh_server *srv, const char *docroot, int port, const char *index_file) {
    srv->docroot = strdup(docroot);
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

void vh_close(vh_server *srv) {
    if (srv->listen_fd >= 0) {
        close(srv->listen_fd);
        srv->listen_fd = -1;
    }
    free((void*)srv->docroot);
    free((void*)srv->index_file);
    srv->docroot = NULL;
    srv->index_file = NULL;
}

int vh_serve_once(vh_server *srv) {
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
        vh_send_error(client_fd, 400, "Bad Request", 0);
        close(client_fd);
        return 0;
    }
    
    char method[16], path[MAX_PATH_LEN], version[16];
    if (sscanf(buffer, "%15s %4095s %15s", method, path, version) != 3) {
        vh_send_error(client_fd, 400, "Bad Request", 0);
        close(client_fd);
        return 0;
    }
    
    int is_head = 0;
    if (strcmp(method, "GET") == 0) {
        is_head = 0;
    } else if (strcmp(method, "HEAD") == 0) {
        is_head = 1;
    } else {
        vh_send_error(client_fd, 405, "Method Not Allowed", 0);
        close(client_fd);
        return 0;
    }
    
    char normalized_path[MAX_PATH_LEN];
    if (vh_normalize_path(path, normalized_path) < 0) {
        vh_send_error(client_fd, 404, "Not Found", is_head);
        close(client_fd);
        return 0;
    }
    
    char file_path[PATH_MAX];
    int ret = snprintf(file_path, sizeof(file_path), "%s%s", srv->docroot, normalized_path);
    if ((size_t)ret >= sizeof(file_path)) {
        vh_send_error(client_fd, 404, "Not Found", is_head);
        close(client_fd);
        return 0;
    }
    
    if (strncmp(file_path, srv->docroot, strlen(srv->docroot)) != 0) {
        vh_send_error(client_fd, 404, "Not Found", is_head);
        close(client_fd);
        return 0;
    }
    
    char resolved_path[PATH_MAX];
    if (realpath(file_path, resolved_path)) {
        strcpy(file_path, resolved_path);
    }
    
    struct stat st;
    if (stat(file_path, &st) < 0) {
        vh_send_error(client_fd, 404, "Not Found", is_head);
        close(client_fd);
        return 0;
    }
    
    if (S_ISDIR(st.st_mode)) {
        char temp_path[PATH_MAX];
        ret = snprintf(temp_path, sizeof(temp_path), "%s/%s", file_path, srv->index_file);
        if ((size_t)ret >= sizeof(temp_path)) {
            vh_send_error(client_fd, 404, "Not Found", is_head);
            close(client_fd);
            return 0;
        }
        strcpy(file_path, temp_path);
        
        if (stat(file_path, &st) < 0) {
            vh_send_error(client_fd, 404, "Not Found", is_head);
            close(client_fd);
            return 0;
        }
    }
    
    if (!S_ISREG(st.st_mode)) {
        vh_send_error(client_fd, 404, "Not Found", is_head);
        close(client_fd);
        return 0;
    }
    
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        vh_send_error(client_fd, 500, "Internal Server Error", is_head);
        close(client_fd);
        return 0;
    }
    
    char date_buf[64];
    vh_format_http_date(date_buf, sizeof(date_buf));
    
    const char *mime_type = vh_get_mime_type(file_path);
    
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
