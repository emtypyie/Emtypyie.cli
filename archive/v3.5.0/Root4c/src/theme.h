#ifndef THEME_H
#define THEME_H

typedef struct {
    const char *name;
    const char *main;    /* primary text */
    const char *dim;     /* dim/muted text */
    const char *accent;  /* accent text */
    const char *warn;    /* warning */
    const char *err;     /* error */
} Theme;

extern Theme g_theme;

void theme_init(const char *name);
const char* theme_list(void);
const char* theme_current(void);
const char* retro(const char *text);
const char* retro_dim(const char *text);
const char* retro_accent(const char *text);
const char* retro_warn(const char *text);
const char* retro_err(const char *text);

#endif
