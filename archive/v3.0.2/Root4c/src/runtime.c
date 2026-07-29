#include "runtime.h"
#include "theme.h"
#include "download.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ─── Runtime installer module ───
 * Downloads and installs development runtimes (currently GCC/G++ via w64devkit).
 * Uses 7zr portable extractor for ZIP extraction on Windows.
 *
 * Future work:
 *  - Support installing Node.js, Python, Rust, etc.
 *  - Add version selection (not just latest).
 *  - Add Linux/macOS package manager integration (apt, brew).
 *  - Check for existing installations more thoroughly.
 */

#define W64DEVKIT_URL "https://github.com/skeeto/w64devkit/releases/download/v2.0.0/w64devkit-2.0.0.zip"
#define SEVENZ_URL "https://www.7-zip.org/a/7zr.exe"

void runtime_install_compiler(const char *name) {
    printf("\n");

    if (!is_windows()) {
        printf("  %s\n", retro_err("Runtime installer is Windows-only."));
        printf("  %s\n", retro_dim("Use your system package manager (apt, pacman, brew, etc.)"));
        return;
    }

    printf("  %s %s\n", retro_dim("Checking for existing"), retro_accent(name));

    if (is_in_path(name)) {
        char which_buf[512];
        char which_cmd[128];
        snprintf(which_cmd, sizeof(which_cmd), "where %s", name);
        exec_cmd(which_cmd, which_buf, sizeof(which_buf));
        char *nl = strchr(which_buf, '\n');
        if (nl) *nl = '\0';
        printf("  %s %s %s\n", retro_accent(name), retro_dim("already installed at"), retro_dim(which_buf));
        printf("\n");
        return;
    }

    printf("  %s\n", retro("Downloading 7zr (portable 7-Zip)..."));
    char *tmp = get_temp_dir();
    char sz_path[1024];
    snprintf(sz_path, sizeof(sz_path), "%s%c7zr.exe", tmp, PATH_SEP);

    if (!download_file(SEVENZ_URL, sz_path)) {
        printf("  %s\n", retro_err("Failed to download 7zr."));
        printf("\n");
        return;
    }

    char *runtimes = get_runtimes_dir();
    char w64_dir[1024];
    snprintf(w64_dir, sizeof(w64_dir), "%s%cw64devkit", runtimes, PATH_SEP);

    printf("  %s\n", retro("Downloading w64devkit (GCC/G++ suite)..."));
    char zip_path[1024];
    snprintf(zip_path, sizeof(zip_path), "%s%cw64devkit.zip", tmp, PATH_SEP);

    if (!download_file(W64DEVKIT_URL, zip_path)) {
        printf("  %s\n", retro_err("Failed to download w64devkit."));
        printf("\n");
        return;
    }

    printf("  %s\n", retro("Extracting w64devkit..."));
    char extract_cmd[2048];
    snprintf(extract_cmd, sizeof(extract_cmd), "\"%s\" x \"%s\" -o\"%s\" -y > nul 2>&1", sz_path, zip_path, runtimes);
    exec_cmd_silent(extract_cmd);

    char bin_dir[1024];
    snprintf(bin_dir, sizeof(bin_dir), "%s%cw64devkit%cbin", runtimes, PATH_SEP, PATH_SEP);

    if (dir_exists(bin_dir)) {
        printf("  %s\n", retro("Adding to user PATH (permanent)..."));
        add_to_user_path(bin_dir);

        printf("  %s\n", retro_dim("For current session, run:"));
        printf("  %s\n", retro("Refreshing environment variables..."));
        /* Escape single quotes for PowerShell */
        char esc_bin[4096] = {0};
        size_t j = 0;
        for (const char *p = bin_dir; *p && j < sizeof(esc_bin) - 3; p++) {
            if (*p == '\'') { esc_bin[j++] = '\''; esc_bin[j++] = '\''; }
            else { esc_bin[j++] = *p; }
        }
        esc_bin[j] = '\0';
        char refresh_cmd[8192];
        snprintf(refresh_cmd, sizeof(refresh_cmd),
            "powershell -Command \"$env:Path = '%s;' + [Environment]::GetEnvironmentVariable('Path','User') + ';' + [Environment]::GetEnvironmentVariable('Path','Machine')",
            esc_bin);
        exec_cmd_silent(refresh_cmd);

        printf("\n");
        printf("  %s %s\n", retro_accent(name), retro_dim("installed successfully!"));
        printf("  %s\n", retro_dim("You may need to restart your terminal."));
    } else {
        printf("  %s\n", retro_err("Extraction may have failed — bin directory not found."));
    }

    remove(zip_path);
    printf("\n");
}
