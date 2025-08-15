#define _GNU_SOURCE
#include "vaayu_http.h"
#include <string.h>
#include <strings.h>

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

const char *vh_get_mime_type(const char *path) {
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
