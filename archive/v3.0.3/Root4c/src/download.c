#include "download.h"
#include "theme.h"
#include "fetch.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <curl/curl.h>
#endif

/* ─── File download module ───
 * Downloads files via PowerShell (Windows) or libcurl (POSIX).
 * Also provides download_zip_and_extract() for packaged content.
 *
 * Future work:
 *  - Add progress bar during download.
 *  - Add resume support for interrupted downloads.
 *  - Add checksum verification after download.
 */

bool download_file(const char *url, const char *dest_path) {
    const char *fname = strrchr(url, '/');
    fname = fname ? fname + 1 : url;

    printf("  %s %s\n", retro("Downloading"), retro_dim(fname));

#ifdef _WIN32
    /* Escape single quotes for PowerShell by doubling them */
    char esc_url[4096] = {0}, esc_dest[4096] = {0};
    size_t j = 0;
    for (const char *p = url; *p && j < sizeof(esc_url) - 3; p++) {
        if (*p == '\'') { esc_url[j++] = '\''; esc_url[j++] = '\''; }
        else { esc_url[j++] = *p; }
    }
    esc_url[j] = '\0';
    j = 0;
    for (const char *p = dest_path; *p && j < sizeof(esc_dest) - 3; p++) {
        if (*p == '\'') { esc_dest[j++] = '\''; esc_dest[j++] = '\''; }
        else { esc_dest[j++] = *p; }
    }
    esc_dest[j] = '\0';

    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
        "powershell -Command \"$wc = New-Object System.Net.WebClient; "
        "$wc.Headers.Add('User-Agent', 'emtypyie-cli/3.0.2'); "
        "$wc.DownloadFile('%s', '%s')\"",
        esc_url, esc_dest);
    if (!exec_cmd_silent(cmd)) {
        printf("  %s\n", retro_err("Download failed"));
        return false;
    }
    long long size = 0;
    FILE *f = fopen(dest_path, "rb");
    if (f) { fseek(f, 0, SEEK_END); size = ftell(f); fclose(f); }
    printf("  %s (%lld bytes)\n", retro_dim("Saved"), size);
    return true;
#else
    CURL *curl = curl_easy_init();
    if (!curl) return false;

    FILE *f = fopen(dest_path, "wb");
    if (!f) { curl_easy_cleanup(curl); return false; }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "emtypyie-cli/3.0.2");

    CURLcode rc = curl_easy_perform(curl);
    fclose(f);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        printf("  %s %s\n", retro_err("Download failed:"), curl_easy_strerror(rc));
        remove(dest_path);
        return false;
    }
    printf("  %s\n", retro_dim("Download complete"));
    return true;
#endif
}

bool download_zip_and_extract(const char *url, const char *dest_dir) {
    char tmp_path[1024];
    const char *fname = strrchr(url, '/');
    fname = fname ? fname + 1 : "download.zip";
    snprintf(tmp_path, sizeof(tmp_path), "%s%c%s", get_temp_dir(), PATH_SEP, fname);

    dir_create(dest_dir);
    if (!download_file(url, tmp_path)) return false;

    printf("  %s\n", retro("Extracting..."));
#ifdef _WIN32
    /* Escape single quotes for PowerShell */
    char esc_tmp[4096] = {0}, esc_dir[4096] = {0};
    size_t j = 0;
    for (const char *p = tmp_path; *p && j < sizeof(esc_tmp) - 3; p++) {
        if (*p == '\'') { esc_tmp[j++] = '\''; esc_tmp[j++] = '\''; }
        else { esc_tmp[j++] = *p; }
    }
    esc_tmp[j] = '\0';
    j = 0;
    for (const char *p = dest_dir; *p && j < sizeof(esc_dir) - 3; p++) {
        if (*p == '\'') { esc_dir[j++] = '\''; esc_dir[j++] = '\''; }
        else { esc_dir[j++] = *p; }
    }
    esc_dir[j] = '\0';

    char extract_cmd[8192];
    snprintf(extract_cmd, sizeof(extract_cmd),
        "powershell -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"",
        esc_tmp, esc_dir);
    bool ok = exec_cmd_silent(extract_cmd);
#else
    char extract_cmd[4096];
    snprintf(extract_cmd, sizeof(extract_cmd), "unzip -o '%s' -d '%s'", tmp_path, dest_dir);
    bool ok = (system(extract_cmd) == 0);
#endif
    if (ok) {
        printf("  %s\n", retro_dim("Extraction complete"));
    } else {
        printf("  %s\n", retro_err("Extraction failed"));
    }
    remove(tmp_path);
    return ok;
}
