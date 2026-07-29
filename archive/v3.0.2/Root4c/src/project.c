#include "project.h"
#include "theme.h"
#include "fetch.h"
#include "download.h"
#include "util.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* ─── Project management module ───
 * Lists, fetches, installs, flashes (re-downloads), removes, and opens docs
 * for emtypyie projects.  Project metadata is fetched from cdn.emtypyie.in.
 *
 * JSON parsing is minimal and assumes known response shapes (no full JSON lib).
 *
 * Future work:
 *  - Add a proper JSON parser (cJSON is already in lib/) for robustness.
 *  - Add project search/filter by tags.
 *  - Add dependency resolution between projects.
 *  - Cache metadata locally to avoid repeated network calls.
 */

#define MAX_LINE 1024

/* ---- JSON helpers (simple, for known shapes only) ---- */

char* json_string(const char *json, const char *key) {
    char *p = strstr(json, key);
    if (!p) return NULL;
    p += strlen(key);
    while (*p && *p != '"') p++;
    if (!*p) return NULL;
    p++;
    char *end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = end - p;
    char *val = malloc(len + 1);
    if (!val) return NULL;
    memcpy(val, p, len);
    val[len] = '\0';
    return val;
}

/* ---- project metadata from /{name}/metadata.json ---- */

typedef struct {
    char name[128];
    char version[64];
    char description[512];
    char download[1024];
    char repo[512];
} ProjectMeta;

static int parse_metadata(const char *json, ProjectMeta *m) {
    memset(m, 0, sizeof(*m));
    char *v;
    v = json_string(json, "\"name\":");      if (v) { strncpy(m->name, v, sizeof(m->name)-1); free(v); }
    v = json_string(json, "\"version\":");   if (v) { strncpy(m->version, v, sizeof(m->version)-1); free(v); }
    v = json_string(json, "\"description\":"); if (v) { strncpy(m->description, v, sizeof(m->description)-1); free(v); }
    v = json_string(json, "\"download\":");  if (v) { strncpy(m->download, v, sizeof(m->download)-1); free(v); }
    v = json_string(json, "\"repo\":");      if (v) { strncpy(m->repo, v, sizeof(m->repo)-1); free(v); }
    return strlen(m->name) > 0;
}

/* ---- parse the project list from meta.json ---- */

static void skip_json(const char **pp) {
    int depth = 0;
    do {
        if (**pp == '{' || **pp == '[') depth++;
        else if (**pp == '}' || **pp == ']') depth--;
        else if (**pp == '"') {
            (*pp)++;
            while (**pp && **pp != '"') { if (**pp == '\\' && *(*pp+1)) (*pp)++; (*pp)++; }
        }
        if (depth > 0) (*pp)++;
    } while (depth > 0 && **pp);
    if (**pp) (*pp)++;
}

static void list_from_project_array(const char *json) {
    const char *p = json, *q;
    char name[128] = {0}, version[64] = {0}, desc[512] = {0};

    /* find the first '[' after "projects" — but handle both formats */
    /* format 1: array of strings ["a","b"] */
    /* format 2: {"projects":["a","b"]} or {"projects":[{"name":"a",...}]} */

    /* skip to the first array */
    while (*p && *p != '[') p++;
    if (!*p) return;
    p++; /* skip '[' */

    while (*p && *p != ']') {
        while (*p && *p != '"' && *p != ']') p++;
        if (!*p || *p == ']') break;
        p++; /* skip opening quote */
        if (!*p) break;
        q = strchr(p, '"');
        if (!q) break;

        /* check if next non-space after quote is ':' — object format */
        const char *t = q + 1;
        while (*t && *t == ' ') t++;
        if (*t == ':') {
            /* object: read key-value pairs */
            size_t klen = q - p;
            if (klen < sizeof(name)) {
                memcpy(name, p, klen); name[klen] = '\0';
                p = t + 1;
                while (*p == ' ') p++;
                if (*p == '"') {
                    p++;
                    const char *ve = strchr(p, '"');
                    if (ve) {
                        size_t vlen = ve - p;
                        char val[1024];
                        if (vlen < sizeof(val)) {
                            memcpy(val, p, vlen); val[vlen] = '\0';
                            if      (strcmp(name, "name") == 0)        strncpy(name, val, sizeof(name)-1);
                            else if (strcmp(name, "version") == 0)    strncpy(version, val, sizeof(version)-1);
                            else if (strcmp(name, "description") == 0) strncpy(desc, val, sizeof(desc)-1);
                        }
                        p = ve + 1;
                    }
                }
            }
            skip_json(&p);
            /* at '}' — print and reset */
            printf("  %s", retro_accent(strlen(name) ? name : "?"));
            if (strlen(version)) printf("  %s%s", retro_dim("v"), retro_dim(version));
            if (strlen(desc))    printf("  %s", retro_dim(desc));
            printf("\n");
            name[0] = version[0] = desc[0] = '\0';
            while (*p && *p != ',' && *p != ']') p++;
            if (*p == ',') p++;
        } else {
            /* plain string */
            size_t len = q - p;
            if (len > 0 && len < sizeof(name)) {
                memcpy(name, p, len); name[len] = '\0';
                printf("  %s\n", retro_accent(name));
            }
            p = q + 1;
            while (*p && *p != ',' && *p != ']') p++;
            if (*p == ',') p++;
        }
    }
}

/* ---- public API ---- */

void project_list(void) {
    printf("\n");

    char url[256];
    snprintf(url, sizeof(url), "%s/meta.json", API_BASE);

    FetchResult *res = fetch_get_with_timeout(url, 5);
    if (!res || res->status_code != 200) {
        printf("  %s\n", retro_dim("(Could not fetch project list)"));
        fetch_free(res);
        printf("\n");
        return;
    }

    printf("  %s\n", retro_accent("Available projects:"));
    list_from_project_array(res->body);
    fetch_free(res);
    printf("\n");
}

void project_info(const char *name) {
    printf("\n");
    char url[512];
    snprintf(url, sizeof(url), "%s/%s/metadata.json", API_BASE, name);

    printf("  %s %s...\n", retro_dim("Fetching info for"), retro_accent(name));
    FetchResult *res = fetch_get_with_timeout(url, 10);
    if (!res || res->status_code != 200) {
        printf("  %s %s\n", retro_err("Project not found:"), retro_dim(name));
        fetch_free(res);
        return;
    }

    ProjectMeta m;
    if (parse_metadata(res->body, &m)) {
        printf("  %s  %s\n", retro_accent("Name:"),        retro_dim(m.name));
        printf("  %s  %s\n", retro_accent("Version:"),     retro_dim(m.version));
        printf("  %s  %s\n", retro_accent("Description:"), retro_dim(m.description));
        if (strlen(m.repo))
            printf("  %s  %s\n", retro_accent("Repo:"), retro_dim(m.repo));
    } else {
        printf("  %s\n", res->body);
    }
    fetch_free(res);
    printf("\n");
}

void project_install_deps(const char *dev_dir) {
    char req_path[1024];
    snprintf(req_path, sizeof(req_path), "%s%crequirements.txt", dev_dir, PATH_SEP);
    if (!file_exists(req_path)) return;

    printf("  %s\n", retro_dim("Installing Python dependencies..."));
    char cmd[2048];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "py -3 -m pip install -r \"%s\" -q", req_path);
#else
    snprintf(cmd, sizeof(cmd), "python3 -m pip install -r \"%s\" -q", req_path);
#endif
    if (system(cmd) == 0) {
        printf("  %s\n", retro("Dependencies installed."));
    } else {
        printf("  %s\n", retro_warn("pip install had issues (see above)."));
    }
}

void project_get(const char *name) {
    printf("\n");

    char meta_url[512];
    snprintf(meta_url, sizeof(meta_url), "%s/%s/metadata.json", API_BASE, name);

    printf("  %s %s\n", retro_dim("Fetching"), retro_accent(name));
    FetchResult *meta = fetch_get_with_timeout(meta_url, 10);
    if (!meta || meta->status_code != 200) {
        printf("  %s %s\n", retro_err("Project not found:"), retro_dim(name));
        fetch_free(meta);
        return;
    }

    ProjectMeta m;
    char *meta_body = strdup(meta->body);
    if (!meta_body) {
        printf("  %s\n", retro_err("Out of memory."));
        fetch_free(meta);
        return;
    }
    parse_metadata(meta_body, &m);
    fetch_free(meta);

    if (strlen(m.version))
        printf("  %s %s\n", retro_dim("Version:"), retro_accent(m.version));
    if (strlen(m.repo))
        printf("  %s %s\n", retro_dim("Repo:"),   retro_dim(m.repo));

    char *dev_dir = get_dev_dir(name);
    int installed = 0;

    if (strlen(m.download) > 0) {
        int is_zip = (strstr(m.download, ".zip") != NULL);
        char fname[512];
        /* Prefer an explicit filename from metadata; else derive from the URL. */
        char *mf = json_string(meta_body, "\"filename\":");
        if (mf) { strncpy(fname, mf, sizeof(fname) - 1); fname[sizeof(fname) - 1] = '\0'; free(mf); }
        else {
            const char *base = strrchr(m.download, '/');
            snprintf(fname, sizeof(fname), "%s", base ? base + 1 : "download.bin");
        }
        char dest[1024];
        snprintf(dest, sizeof(dest), "%s%c%s", dev_dir, PATH_SEP, fname);

        if (download_file(m.download, dest)) {
            if (is_zip) {
                printf("  %s\n", retro("Extracting..."));
                int extract_ok = 0;
#ifdef _WIN32
                char extract_cmd[2048];
                snprintf(extract_cmd, sizeof(extract_cmd),
                    "powershell -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"", dest, dev_dir);
                extract_ok = exec_cmd_silent(extract_cmd);
#else
                char extract_cmd[2048];
                snprintf(extract_cmd, sizeof(extract_cmd), "unzip -o '%s' -d '%s'", dest, dev_dir);
                extract_ok = (system(extract_cmd) == 0);
#endif
                remove(dest);
                if (!extract_ok) {
                    if (dir_exists(dev_dir)) dir_remove_recursive(dev_dir);
                    printf("  %s %s\n", retro_err("Failed to install:"), retro_dim(name));
                } else {
                    installed = 1;
                }
            } else {
                /* Non-archive artifact (e.g. an .exe) — keep it as the run target. */
                installed = 1;
            }
            if (installed) {
                char meta_file[1024];
                snprintf(meta_file, sizeof(meta_file), "%s%c.emtypyie.json", dev_dir, PATH_SEP);
                write_file(meta_file, meta_body);
            }
        } else {
            printf("  %s %s\n", retro_err("Download failed:"), retro_dim(name));
        }

        if (installed) {
            project_install_deps(dev_dir);
            printf("  %s %s %s\n", retro_accent(name), retro_dim("installed to"), retro_dim(dev_dir));
        }
    } else {
        /* no download URL — maybe a meta-only project */
        printf("  %s %s\n", retro_accent(name), retro_dim("(meta only, nothing to download)"));
    }

    if (strlen(m.description)) {
        printf("\n  %s\n", retro_dim(m.description));
    }

    free(meta_body);
    printf("\n");
}

void project_flash(const char *name) {
    char *dev_dir = get_dev_dir(name);
    if (dir_exists(dev_dir)) {
        dir_remove_recursive(dev_dir);
    }
    project_get(name);
}

void project_remove(const char *name) {
    char *dev_dir = get_dev_dir(name);
    if (dir_exists(dev_dir)) {
        dir_remove_recursive(dev_dir);
        printf("  %s %s\n", retro_accent(name), retro_dim("removed."));
    } else {
        printf("  %s %s\n", retro_dim("Project not found:"), retro_dim(name));
    }
}

void project_docs(const char *name) {
    printf("  %s\n", retro_dim("Opening docs..."));
    /* Validate name contains only safe characters (alphanumeric, hyphen, underscore) */
    for (const char *p = name; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_')) {
            printf("  %s\n", retro_err("Invalid project name."));
            return;
        }
    }
    char url[512];
    snprintf(url, sizeof(url), "https://wiki.emtypyie.in/docs/%s", name);
    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", url);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\"", url);
#endif
    system(cmd);
}

bool project_run(const char *name) {
    char url[512];
    snprintf(url, sizeof(url), "%s/%s/metadata.json", API_BASE, name);

    printf("  %s %s\n", retro_dim("Checking project:"), retro_accent(name));
    FetchResult *res = fetch_get_with_timeout(url, 5);
    if (!res || res->status_code != 200) {
        printf("  %s  %s\n", retro_err("Unknown command:"), retro_dim(name));
        printf("  %s\n", retro_dim("Type /help for available commands."));
        fetch_free(res);
        return false;
    }

    cJSON *json = cJSON_Parse(res->body);
    fetch_free(res);

    if (!json) {
        printf("  %s\n", retro_dim("(Could not parse project metadata)"));
        return false;
    }

    cJSON *run_item = cJSON_GetObjectItem(json, "run");
    const char *run_val = cJSON_GetStringValue(run_item);
    if (!run_val || strlen(run_val) == 0) {
        run_item = cJSON_GetObjectItem(json, "filename");
        run_val = cJSON_GetStringValue(run_item);
    }

    if (!run_val || strlen(run_val) == 0) {
        printf("  %s %s\n", retro_err("Project has no run target:"), retro_dim(name));
        cJSON_Delete(json);
        return false;
    }

    char *dev_dir = get_dev_dir(name);
    char run_path[1024];
    snprintf(run_path, sizeof(run_path), "%s%c%s", dev_dir, PATH_SEP, run_val);

    if (!file_exists(run_path)) {
        printf("  %s %s %s\n", retro_err("Project not installed."), retro_dim("Run /get"), retro_accent(name));
        cJSON_Delete(json);
        return false;
    }

    project_install_deps(dev_dir);

    char spawn_cmd[2048];
    const char *ext = strrchr(run_val, '.');
    if (ext && (strcmp(ext, ".py") == 0)) {
        snprintf(spawn_cmd, sizeof(spawn_cmd), "py -3 \"%s\"", run_path);
    } else if (ext && (strcmp(ext, ".bat") == 0 || strcmp(ext, ".cmd") == 0)) {
        snprintf(spawn_cmd, sizeof(spawn_cmd), "\"%s\"", run_path);
    } else if (ext && (strcmp(ext, ".sh") == 0)) {
        snprintf(spawn_cmd, sizeof(spawn_cmd), "sh \"%s\"", run_path);
    } else {
        snprintf(spawn_cmd, sizeof(spawn_cmd), "\"%s\"", run_path);
    }

printf("\n");
    int rc = system(spawn_cmd);
    printf("\n");
    if (rc != 0) {
        printf("  %s (exit code %d)\n", retro_err("Project exited with error:"), rc);
    }

    cJSON_Delete(json);
    return true;
}
