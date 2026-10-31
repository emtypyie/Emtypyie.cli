#ifndef FETCH_H
#define FETCH_H

#include <stddef.h>

typedef struct {
    int status_code;
    char *body;
    size_t body_size;
    char error[256];       /* populated on failure with human-readable error */
} FetchResult;

FetchResult* fetch_get(const char *url);
FetchResult* fetch_get_with_timeout(const char *url, int timeout_sec);
void fetch_free(FetchResult *res);

#endif
