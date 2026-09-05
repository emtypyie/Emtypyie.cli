#include "template.h"
#include "theme.h"
#include "fetch.h"
#include "util.h"
#include "cJSON.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEMPLATE_API_BASE "/dev/templates"

static bool download_template_metadata(const char *template_name, cJSON **out_json) {
    char url[512];
    snprintf(url, sizeof(url), "https://cdn.emtypyie.in%s/%s/metadata.json", TEMPLATE_API_BASE, template_name);
    
    FetchResult *res = fetch_get_with_timeout(url, 10);
    if (!res || res->status_code != 200) {
        if (res) fetch_free(res);
        return false;
    }
    
    *out_json = cJSON_Parse(res->body);
    fetch_free(res);
    return *out_json != NULL;
}

static char *replace_vars(const char *input, const char *project_name, const char *description) {
    if (!input) return NULL;
    
    size_t len = strlen(input);
    char *result = malloc(len * 2 + 1);
    if (!result) return NULL;
    
    const char *p = input;
    char *q = result;
    
    while (*p) {
        if (p[0] == '{' && p[1] == '{') {
            const char *end = strstr(p, "}}");
            if (end) {
                size_t var_len = end - p - 2;
                if (var_len == 4 && strncmp(p + 2, "name", 4) == 0) {
                    strcpy(q, project_name);
                    q += strlen(project_name);
                } else if (var_len == 11 && strncmp(p + 2, "description", 11) == 0) {
                    strcpy(q, description);
                    q += strlen(description);
                } else {
                    // Unknown variable, copy as-is
                    *q++ = '{';
                    *q++ = '{';
                    memcpy(q, p + 2, var_len);
                    q += var_len;
                    *q++ = '}';
                    *q++ = '}';
                }
                p = end + 2;
            } else {
                *q++ = *p++;
            }
        } else {
            *q++ = *p++;
        }
    }
    *q = '\0';
    return result;
}

void template_list(void) {
    printf("\n");
    printf("  %s\n", retro_dim("─── Templates ───"));
    printf("\n");
    
    const char *templates[][2] = {
        {"python-cli", "Python CLI application with Click"},
        {"node-cli", "Node.js CLI application with Commander"},
        {"rust-cli", "Rust CLI application with Clap"}
    };
    
    for (int i = 0; i < 3; i++) {
        printf("  %s  %s\n", retro_accent(templates[i][0]), retro_dim(templates[i][1]));
    }
    
    printf("\n");
}

bool template_create(const char *template_name, const char *project_name, const char *target_dir) {
    cJSON *template_json = NULL;
    
    if (!download_template_metadata(template_name, &template_json)) {
        printf("  %s Template not found: %s\n", retro_err("Error:"), template_name);
        return false;
    }
    
    cJSON *files = cJSON_GetObjectItem(template_json, "files");
    cJSON *postCreate = cJSON_GetObjectItem(template_json, "postCreate");
    cJSON *desc = cJSON_GetObjectItem(template_json, "description");
    
    const char *description = cJSON_IsString(desc) ? desc->valuestring : "";
    
    char full_path[1024];
    if (target_dir && strlen(target_dir) > 0) {
        snprintf(full_path, sizeof(full_path), "%s%c%s", target_dir, PATH_SEP, project_name);
    } else {
        snprintf(full_path, sizeof(full_path), "%s", project_name);
    }
    
    if (dir_exists(full_path)) {
        printf("  %s Directory already exists: %s\n", retro_err("Error:"), full_path);
        cJSON_Delete(template_json);
        return false;
    }
    
    printf("  %s %s from %s...\n", retro("Creating"), retro_accent(project_name), retro_accent(template_name));
    
    if (!dir_create(full_path)) {
        printf("  %s Failed to create directory\n", retro_err("Error:"));
        cJSON_Delete(template_json);
        return false;
    }
    
    int file_count = cJSON_GetArraySize(files);
    for (int i = 0; i < file_count; i++) {
        cJSON *file = cJSON_GetArrayItem(files, i);
        cJSON *path = cJSON_GetObjectItem(file, "path");
        cJSON *content = cJSON_GetObjectItem(file, "content");
        cJSON *executable = cJSON_GetObjectItem(file, "executable");
        
        if (!cJSON_IsString(path) || !cJSON_IsString(content)) continue;
        
        char *rendered_content = replace_vars(content->valuestring, project_name, description);
        if (!rendered_content) continue;
        
        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "%s%c%s", full_path, PATH_SEP, path->valuestring);
        
        char *dir = strdup(file_path);
        char *last_slash = strrchr(dir, PATH_SEP);
        if (last_slash) {
            *last_slash = '\0';
            if (!dir_exists(dir)) {
                // Create nested directories
                char *p = dir;
                while (*p) {
                    if (*p == PATH_SEP) {
                        *p = '\0';
                        dir_create(dir);
                        *p = PATH_SEP;
                    }
                    p++;
                }
                dir_create(dir);
            }
        }
        free(dir);
        
        if (write_file(file_path, rendered_content)) {
            printf("  %s %s\n", retro_dim("Created"), path->valuestring);
        } else {
            printf("  %s %s\n", retro_warn("Failed to create"), path->valuestring);
        }
        
        if (cJSON_IsTrue(executable)) {
#ifdef _WIN32
            // On Windows, executable flag doesn't apply the same way
#else
            chmod(file_path, 0755);
#endif
        }
        
        free(rendered_content);
    }
    
    if (cJSON_IsString(postCreate)) {
        printf("  %s\n", retro_dim("Running post-create script..."));
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "cd \"%s\" && %s", full_path, postCreate->valuestring);
        system(cmd);
        printf("  %s\n", retro("Post-create completed."));
    }
    
    printf("\n");
    printf("  %s Project created at %s\n", retro("✓"), retro_accent(full_path));
    printf("  %s cd %s && %s\n", retro_dim("Next:"), project_name, postCreate && cJSON_IsString(postCreate) ? postCreate->valuestring : "# start coding");
    printf("\n");
    
    cJSON_Delete(template_json);
    return true;
}

void template_handle_command(const char *args) {
    if (!args || strlen(args) == 0) {
        template_list();
        return;
    }
    
    char *args_copy = strdup(args);
    char *subcommand = strtok(args_copy, " \t");
    
    if (!subcommand) {
        template_list();
        free(args_copy);
        return;
    }
    
    if (strcmp(subcommand, "list") == 0) {
        template_list();
    } else if (strcmp(subcommand, "create") == 0 || strcmp(subcommand, "init") == 0) {
        char *template_name = strtok(NULL, " \t");
        char *project_name = strtok(NULL, " \t");
        char *target_dir = strtok(NULL, " \t");
        
        if (!template_name || !project_name) {
            printf("  %s Usage: /new <template> <project-name> [target-dir]\n", retro_err("Error:"));
            printf("  %s    Or: /init <template> <project-name> [target-dir]\n", retro_dim("Usage:"));
        } else {
            template_create(template_name, project_name, target_dir);
        }
    } else {
        printf("  %s Unknown template command: %s\n", retro_err("Error:"), subcommand);
        printf("  %s Available: list, create, init\n", retro_dim("Usage:"));
    }
    
    free(args_copy);
}