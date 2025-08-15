#ifndef VAAYU_HTTP_H
#define VAAYU_HTTP_H

#include <stddef.h>

typedef struct {
    int listen_fd;
    const char *docroot;      // normalized absolute path
    const char *index_file;   // e.g., "index.html"
    int port;                 // e.g., 8080
    size_t max_header_bytes;  // e.g., 16384
} vh_server;

int  vh_init(vh_server *srv, const char *docroot, int port, const char *index_file);
void vh_close(vh_server *srv);
int  vh_serve_once(vh_server *srv); // accept→handle one request→close; returns 0/errno-style

// Internal API (used by other modules)
const char *vh_get_mime_type(const char *path);
void vh_format_http_date(char *buf, size_t len);
int vh_send_error(int fd, int status, const char *reason, int is_head);
int vh_normalize_path(const char *request_path, char *normalized);

#endif // VAAYU_HTTP_H
