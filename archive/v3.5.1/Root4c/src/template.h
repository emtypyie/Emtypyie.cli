#ifndef TEMPLATE_H
#define TEMPLATE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void template_list(void);
bool template_create(const char *template_name, const char *project_name, const char *target_dir);
void template_handle_command(const char *args);

#ifdef __cplusplus
}
#endif

#endif