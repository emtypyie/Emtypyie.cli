#include "verify.h"
#include "theme.h"
#include "util.h"
#include "fetch.h"
#include "download.h"
#include "project.h"
#include "sha256.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

static char *get_manifest_path(const char *project_name) {
    char *dev_dir = get_dev_dir(project_name);
    static char path[1024];
    snprintf(path, sizeof(path), "%s%c.emtypyie-manifest.json", dev_dir, PATH_SEP);
    return path;
}

static ManifestFile *parse_manifest_files(cJSON *files_array, int *count) {
    if (!cJSON_IsArray(files_array)) {
        *count = 0;
        return NULL;
    }
    
    int n = cJSON_GetArraySize(files_array);
    if (n == 0) {
        *count = 0;
        return NULL;
    }
    
    ManifestFile *files = calloc(n, sizeof(ManifestFile));
    if (!files) {
        *count = 0;
        return NULL;
    }
    
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(files_array, i);
        cJSON *path = cJSON_GetObjectItem(item, "path");
        cJSON *sha256 = cJSON_GetObjectItem(item, "sha256");
        cJSON *size = cJSON_GetObjectItem(item, "size");
        cJSON *expected_sha256 = cJSON_GetObjectItem(item, "expectedSha256");
        cJSON *expected_size = cJSON_GetObjectItem(item, "expectedSize");
        
        if (cJSON_IsString(path)) strncpy(files[i].path, path->valuestring, sizeof(files[i].path) - 1);
        if (cJSON_IsString(sha256)) strncpy(files[i].sha256, sha256->valuestring, sizeof(files[i].sha256) - 1);
        if (cJSON_IsNumber(size)) files[i].size = (size_t)size->valuedouble;
        if (cJSON_IsString(expected_sha256)) strncpy(files[i].expected_sha256, expected_sha256->valuestring, sizeof(files[i].expected_sha256) - 1);
        if (cJSON_IsNumber(expected_size)) files[i].expected_size = (size_t)expected_size->valuedouble;
    }
    
    *count = n;
    return files;
}

static cJSON *create_manifest_files_json(ManifestFile *files, int count) {
    cJSON *array = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "path", files[i].path);
        cJSON_AddStringToObject(item, "sha256", files[i].sha256);
        cJSON_AddNumberToObject(item, "size", (double)files[i].size);
        if (files[i].expected_sha256[0]) cJSON_AddStringToObject(item, "expectedSha256", files[i].expected_sha256);
        if (files[i].expected_size > 0) cJSON_AddNumberToObject(item, "expectedSize", (double)files[i].expected_size);
        cJSON_AddItemToArray(array, item);
    }
    return array;
}

bool verify_generate_manifest(const char *project_name, cJSON *project_meta) {
    cJSON *integrity = cJSON_GetObjectItem(project_meta, "integrity");
    if (!integrity) return false;
    
    cJSON *files_array = cJSON_GetObjectItem(integrity, "files");
    if (!files_array) return false;
    
    char *dev_dir = get_dev_dir(project_name);
    int expected_count = 0;
    ManifestFile *expected_files = parse_manifest_files(files_array, &expected_count);
    if (!expected_files || expected_count == 0) {
        free(expected_files);
        return false;
    }
    
    ManifestFile *actual_files = calloc(expected_count, sizeof(ManifestFile));
    if (!actual_files) {
        free(expected_files);
        return false;
    }
    
    int actual_count = 0;
    for (int i = 0; i < expected_count; i++) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s%c%s", dev_dir, PATH_SEP, expected_files[i].path);
        
        if (!file_exists(full_path)) continue;
        
        strncpy(actual_files[actual_count].path, expected_files[i].path, sizeof(actual_files[actual_count].path) - 1);
        
        struct stat st;
        stat(full_path, &st);
        actual_files[actual_count].size = (size_t)st.st_size;
        
        if (expected_files[i].expected_sha256[0]) {
            strncpy(actual_files[actual_count].expected_sha256, expected_files[i].expected_sha256, 
                    sizeof(actual_files[actual_count].expected_sha256) - 1);
        }
        actual_files[actual_count].expected_size = expected_files[i].expected_size;
        
        if (sha256_file(full_path, actual_files[actual_count].sha256)) {
            actual_count++;
        } else {
            // Hash failed, clear it
            actual_files[actual_count].sha256[0] = '\0';
            actual_count++;
        }
    }
    
    ProjectManifest manifest;
    memset(&manifest, 0, sizeof(manifest));
    strncpy(manifest.project, project_name, sizeof(manifest.project) - 1);
    
    cJSON *version = cJSON_GetObjectItem(project_meta, "version");
    if (cJSON_IsString(version)) strncpy(manifest.version, version->valuestring, sizeof(manifest.version) - 1);
    
    time_t now = time(NULL);
    strftime(manifest.installed_at, sizeof(manifest.installed_at), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    
    manifest.files = actual_files;
    manifest.file_count = actual_count;
    
    // Create JSON for manifest hash
    cJSON *manifest_json = cJSON_CreateObject();
    cJSON_AddStringToObject(manifest_json, "project", manifest.project);
    cJSON_AddStringToObject(manifest_json, "version", manifest.version);
    cJSON_AddStringToObject(manifest_json, "installedAt", manifest.installed_at);
    cJSON_AddItemToObject(manifest_json, "files", create_manifest_files_json(actual_files, actual_count));
    
    char *json_str = cJSON_PrintUnformatted(manifest_json);
    if (json_str) {
        sha256_string(json_str, manifest.manifest_sha256);
        free(json_str);
    }
    cJSON_Delete(manifest_json);
    
    // Write manifest
    cJSON *out_json = cJSON_CreateObject();
    cJSON_AddStringToObject(out_json, "project", manifest.project);
    cJSON_AddStringToObject(out_json, "version", manifest.version);
    cJSON_AddStringToObject(out_json, "installedAt", manifest.installed_at);
    cJSON_AddItemToObject(out_json, "files", create_manifest_files_json(actual_files, actual_count));
    cJSON_AddStringToObject(out_json, "manifestSha256", manifest.manifest_sha256);
    
    char *out_str = cJSON_Print(out_json);
    if (out_str) {
        char *manifest_path = get_manifest_path(project_name);
        write_file(manifest_path, out_str);
        free(out_str);
    }
    cJSON_Delete(out_json);
    
    free(expected_files);
    free(actual_files);
    return true;
}

ProjectManifest *verify_load_manifest(const char *project_name) {
    char *manifest_path = get_manifest_path(project_name);
    char *content = read_file(manifest_path);
    if (!content) return NULL;
    
    cJSON *json = cJSON_Parse(content);
    free(content);
    if (!json) return NULL;
    
    ProjectManifest *manifest = calloc(1, sizeof(ProjectManifest));
    if (!manifest) { cJSON_Delete(json); return NULL; }
    
    cJSON *project = cJSON_GetObjectItem(json, "project");
    cJSON *version = cJSON_GetObjectItem(json, "version");
    cJSON *installed_at = cJSON_GetObjectItem(json, "installedAt");
    cJSON *files = cJSON_GetObjectItem(json, "files");
    cJSON *manifest_sha256 = cJSON_GetObjectItem(json, "manifestSha256");
    
    if (cJSON_IsString(project)) strncpy(manifest->project, project->valuestring, sizeof(manifest->project) - 1);
    if (cJSON_IsString(version)) strncpy(manifest->version, version->valuestring, sizeof(manifest->version) - 1);
    if (cJSON_IsString(installed_at)) strncpy(manifest->installed_at, installed_at->valuestring, sizeof(manifest->installed_at) - 1);
    if (cJSON_IsString(manifest_sha256)) strncpy(manifest->manifest_sha256, manifest_sha256->valuestring, sizeof(manifest->manifest_sha256) - 1);
    
    manifest->files = parse_manifest_files(files, &manifest->file_count);
    
    cJSON_Delete(json);
    return manifest;
}

VerifyResult *verify_project(const char *project_name, bool quick) {
    ProjectManifest *manifest = verify_load_manifest(project_name);
    if (!manifest) {
        VerifyResult *result = calloc(1, sizeof(VerifyResult));
        result->ok = false;
        result->mismatched = 0;
        result->missing = 1;
        result->missing_files = calloc(1, sizeof(char*));
        result->missing_files[0] = strdup("No manifest found. Run /get first or /rebuild to create manifest.");
        return result;
    }
    
    char *dev_dir = get_dev_dir(project_name);
    VerifyResult *result = calloc(1, sizeof(VerifyResult));
    result->ok = true;
    result->checked = 0;
    result->mismatched = 0;
    result->missing = 0;
    result->mismatched_files = NULL;
    result->missing_files = NULL;
    
    for (int i = 0; i < manifest->file_count; i++) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s%c%s", dev_dir, PATH_SEP, manifest->files[i].path);
        
        if (!file_exists(full_path)) {
            result->ok = false;
            result->missing++;
            result->missing_files = realloc(result->missing_files, result->missing * sizeof(char*));
            result->missing_files[result->missing - 1] = strdup(manifest->files[i].path);
            continue;
        }
        
        struct stat st;
        stat(full_path, &st);
        if (st.st_size != (long)manifest->files[i].size) {
            result->ok = false;
            result->mismatched++;
            result->mismatched_files = realloc(result->mismatched_files, result->mismatched * sizeof(char*));
            char *msg = malloc(strlen(manifest->files[i].path) + 32);
            snprintf(msg, strlen(manifest->files[i].path) + 32, "%s (size mismatch)", manifest->files[i].path);
            result->mismatched_files[result->mismatched - 1] = msg;
            continue;
        }
        
        if (!quick && manifest->files[i].sha256[0]) {
            char hash[65];
            if (sha256_file(full_path, hash)) {
                if (strcmp(hash, manifest->files[i].sha256) != 0) {
                    result->ok = false;
                    result->mismatched++;
                    result->mismatched_files = realloc(result->mismatched_files, result->mismatched * sizeof(char*));
                    char *msg = malloc(strlen(manifest->files[i].path) + 32);
                    snprintf(msg, strlen(manifest->files[i].path) + 32, "%s (hash mismatch)", manifest->files[i].path);
                    result->mismatched_files[result->mismatched - 1] = msg;
                    continue;
                }
            }
        }
        
        result->checked++;
    }
    
    verify_free_manifest(manifest);
    return result;
}

void verify_show_results(VerifyResult *result, const char *project_name) {
    if (result->ok) {
        printf("  %s %s: All %d files verified OK\n", retro("✓"), project_name, result->checked);
    } else {
        printf("  %s %s: Integrity check FAILED\n", retro_err("✗"), project_name);
        if (result->missing > 0) {
            printf("  %s Missing files (%d):\n", retro_warn("⚠"), result->missing);
            for (int i = 0; i < result->missing; i++) {
                printf("    %s - %s\n", retro_warn("-"), result->missing_files[i]);
            }
        }
        if (result->mismatched > 0) {
            printf("  %s Mismatched files (%d):\n", retro_warn("⚠"), result->mismatched);
            for (int i = 0; i < result->mismatched; i++) {
                printf("    %s - %s\n", retro_warn("-"), result->mismatched_files[i]);
            }
        }
        printf("\n");
        printf("  %s %s has been tampered. Please run /%s rebuild for smooth experience\n", 
               retro_warn("⚠"), project_name, project_name);
    }
}

bool verify_rebuild_project(const char *project_name) {
    printf("  %s %s...\n", retro("Rebuilding"), retro_accent(project_name));
    printf("\n");
    
    char *dev_dir = get_dev_dir(project_name);
    if (dir_exists(dev_dir)) {
        dir_remove_recursive(dev_dir);
        printf("  %s\n", retro_dim("Removed old files."));
    }
    
    extern void project_get(const char*);
    project_get(project_name);
    
    // Fetch fresh metadata and generate manifest
    char url[512];
    snprintf(url, sizeof(url), "%s/%s/metadata.json", API_BASE, project_name);
    FetchResult *res = fetch_get_with_timeout(url, 10);
    if (res && res->status_code == 200) {
        cJSON *json = cJSON_Parse(res->body);
        if (json) {
            verify_generate_manifest(project_name, json);
            cJSON_Delete(json);
        }
        fetch_free(res);
    }
    
    printf("  %s Rebuild complete!\n", retro("✓"));
    printf("\n");
    return true;
}

void verify_free_manifest(ProjectManifest *manifest) {
    if (!manifest) return;
    if (manifest->files) free(manifest->files);
    free(manifest);
}

void verify_free_result(VerifyResult *result) {
    if (!result) return;
    if (result->mismatched_files) {
        for (int i = 0; i < result->mismatched; i++) free(result->mismatched_files[i]);
        free(result->mismatched_files);
    }
    if (result->missing_files) {
        for (int i = 0; i < result->missing; i++) free(result->missing_files[i]);
        free(result->missing_files);
    }
    free(result);
}