#ifndef TINYHTTP_H
#define TINYHTTP_H

#include <stddef.h>

typedef struct {
    int listen_fd;
    const char *docroot;      // normalized absolute path
    const char *index_file;   // e.g., "index.html"
    int port;                 // e.g., 8080
    size_t max_header_bytes;  // e.g., 16384
} th_server;

int  th_init(th_server *srv, const char *docroot, int port, const char *index_file);
void th_close(th_server *srv);
int  th_serve_once(th_server *srv); // accept→handle one request→close; returns 0/errno-style

#endif // TINYHTTP_H
