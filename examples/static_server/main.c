#include "vaayu_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>

static vh_server server;
static volatile int running = 1;

static void sigint_handler(int sig) {
    (void)sig;
    running = 0;
    vh_close(&server);
    exit(0);
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-p PORT] [-r DOCROOT] [-i INDEX] [-H MAX_HEADER_BYTES]\n", prog);
    fprintf(stderr, "  -p PORT           Listen port (default: 8080)\n");
    fprintf(stderr, "  -r DOCROOT        Document root directory (default: ./www)\n");
    fprintf(stderr, "  -i INDEX          Index file name (default: index.html)\n");
    fprintf(stderr, "  -H MAX_HEADER     Max header bytes (default: 16384)\n");
}

int main(int argc, char *argv[]) {
    int port = 8080;
    const char *docroot = "./www";
    const char *index_file = "index.html";
    size_t max_header_bytes = 16384;
    
    int opt;
    while ((opt = getopt(argc, argv, "p:r:i:H:h")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Invalid port: %s\n", optarg);
                    return 1;
                }
                break;
            case 'r':
                docroot = optarg;
                break;
            case 'i':
                index_file = optarg;
                break;
            case 'H':
                max_header_bytes = (size_t)atoi(optarg);
                if (max_header_bytes == 0) {
                    fprintf(stderr, "Invalid max header bytes: %s\n", optarg);
                    return 1;
                }
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            default:
                usage(argv[0]);
                return 1;
        }
    }
    
    // Initialize server
    int ret = vh_init(&server, docroot, port, index_file);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize server: %s\n", strerror(ret));
        return 1;
    }
    
    server.max_header_bytes = max_header_bytes;
    
    // Set up signal handler for graceful shutdown
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    
    printf("Starting HTTP server on port %d, serving %s\n", port, docroot);
    
    // Main server loop
    while (running) {
        ret = vh_serve_once(&server);
        if (ret != 0 && running) {
            fprintf(stderr, "Error serving request: %s\n", strerror(ret));
        }
    }
    
    vh_close(&server);
    return 0;
}
