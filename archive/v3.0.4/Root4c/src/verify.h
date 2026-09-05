#ifndef VERIFY_H
#define VERIFY_H

#include <stdbool.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char path[512];
    char sha256[65];
    size_t size;
    char expected_sha256[65];
    size_t expected_size;
} ManifestFile;

typedef struct {
    char project[128];
    char version[64];
    char installed_at[64];
    ManifestFile *files;
    int file_count;
    char manifest_sha256[65];
} ProjectManifest;

typedef struct {
    bool ok;
    int checked;
    int mismatched;
    int missing;
    char **mismatched_files;
    char **missing_files;
} VerifyResult;

/**
 * Generate manifest for a project after installation
 * @param project_name Project name
 * @param project_meta Project metadata JSON (cJSON object)
 * @return true on success
 */
bool verify_generate_manifest(const char *project_name, cJSON *project_meta);

/**
 * Load manifest from disk
 * @param project_name Project name
 * @return ProjectManifest (must be freed with verify_free_manifest)
 */
ProjectManifest *verify_load_manifest(const char *project_name);

/**
 * Verify project integrity
 * @param project_name Project name
 * @param quick If true, only check file sizes (not hashes)
 * @return VerifyResult (must be freed with verify_free_result)
 */
VerifyResult *verify_project(const char *project_name, bool quick);

/**
 * Show verification results
 * @param result VerifyResult
 * @param project_name Project name
 */
void verify_show_results(VerifyResult *result, const char *project_name);

/**
 * Rebuild project (re-download and regenerate manifest)
 * @param project_name Project name
 * @return true on success
 */
bool verify_rebuild_project(const char *project_name);

/**
 * Free ProjectManifest
 */
void verify_free_manifest(ProjectManifest *manifest);

/**
 * Free VerifyResult
 */
void verify_free_result(VerifyResult *result);

#ifdef __cplusplus
}
#endif

#endif