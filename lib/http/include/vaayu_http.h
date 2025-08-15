#ifndef VAAYU_HTTP_H
#define VAAYU_HTTP_H

#include <stddef.h>

typedef struct {
    int listen_fd;
    const char *docroot;
    const char *index_file;
    int port;
    size_t max_header_bytes;
} vh_server;

int  vh_init(vh_server *srv, const char *docroot, int port, const char *index_file);
void vh_close(vh_server *srv);
int  vh_serve_once(vh_server *srv);

const char *vh_get_mime_type(const char *path);
void vh_format_http_date(char *buf, size_t len);
int vh_send_error(int fd, int status, const char *reason, int is_head);
int vh_normalize_path(const char *request_path, char *normalized);

#endif
