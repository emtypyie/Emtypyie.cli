#ifndef PROJECT_H
#define PROJECT_H

#include <stdbool.h>

#define API_BASE "https://cdn.emtypyie.in/dev"

/* Simple JSON string extractor (returns malloc'd copy or NULL). */
char* json_string(const char *json, const char *key);

void project_list(void);
void project_info(const char *name);
void project_get(const char *name);
void project_flash(const char *name);
void project_remove(const char *name);
void project_docs(const char *name);

/* Run an installed project by name.
 * Fetches metadata from CDN, extracts the "run" entry,
 * and spawns the corresponding script/executable detached.
 * Returns true if the project was launched. */
bool project_run(const char *name);

#endif
