#ifndef LARPINO_H
#define LARPINO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct larpino_model larpino_model;

larpino_model* larpino_load(const char *path);
void           larpino_free(larpino_model *m);
int            larpino_is_loaded(const larpino_model *m);

typedef void (*larpino_callback)(const char *token, void *user);

int  larpino_chat(larpino_model *m, const char *prompt, larpino_callback cb, void *user);
void larpino_stop(larpino_model *m);

const char* larpino_status(larpino_model *m, char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif
