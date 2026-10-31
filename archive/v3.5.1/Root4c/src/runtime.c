#include "runtime.h"
#include "theme.h"
#include "download.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ─── Runtime installer module ───
 * Downloads and installs development runtimes (GCC/G++ via w64devkit and an
 * isolated local Python). Uses 7zr portable extractor for ZIP extraction.
 *
 * Future work:
 *  - Support installing Node.js, Rust, etc.
 *  - Add version selection (not just latest).
 *  - Add Linux/macOS package manager integration (apt, brew).
 *  - Check for existing installations more thoroughly.
 */

#define W64DEVKIT_URL "https://github.com/skeeto/w64devkit/releases/download/v2.0.0/w64devkit-2.0.0.zip"
#define SEVENZ_URL "https://www.7-zip.org/a/7zr.exe"

/* Local, isolated Python runtime. Never touches system/global installs. */
#define PYTHON_VERSION "3.12.8"
#define PYTHON_EMBED_URL "https://www.python.org/ftp/python/3.12.8/python-3.12.8-embed-amd64.zip"
#define GET_PIP_URL "https://bootstrap.pypa.io/get-pip.py"

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

/* ─── Local Python runtime ───
 * Windows: extracts the official embeddable package (no installer, no
 * registry, no PATH changes outside this process) into
 * ~/.emtypyie/runtimes/python, enables site-packages and bootstraps pip.
 * POSIX: creates a venv at the same location from the system python3.
 */

#ifdef _WIN32
static void ps_escape(const char *in, char *out, size_t out_sz) {
    size_t j = 0;
    for (const char *p = in; *p && j + 2 < out_sz; p++) {
        if (*p == '\'') { out[j++] = '\''; out[j++] = '\''; }
        else { out[j++] = *p; }
    }
    out[j] = '\0';
}
#endif

static void python_paths(char *dir, size_t dir_sz, char *exe, size_t exe_sz,
                         char *scripts, size_t scripts_sz) {
    char *runtimes = get_runtimes_dir();
    snprintf(dir, dir_sz, "%s%cpython", runtimes, PATH_SEP);
#ifdef _WIN32
    snprintf(exe, exe_sz, "%s%cpython.exe", dir, PATH_SEP);
    snprintf(scripts, scripts_sz, "%s%cScripts", dir, PATH_SEP);
#else
    snprintf(exe, exe_sz, "%s%cbin%cpython3", dir, PATH_SEP, PATH_SEP);
    snprintf(scripts, scripts_sz, "%s%cbin", dir, PATH_SEP);
#endif
}

bool runtime_python_installed(void) {
    char dir[1024], exe[1024], scripts[1024];
    python_paths(dir, sizeof(dir), exe, sizeof(exe), scripts, sizeof(scripts));
    return file_exists(exe);
}

const char *runtime_python_exe(void) {
    static char exe[1024];
    char dir[1024], scripts[1024];
    python_paths(dir, sizeof(dir), exe, sizeof(exe), scripts, sizeof(scripts));
    return exe;
}

void runtime_apply_python_path(void) {
    char dir[1024], exe[1024], scripts[1024];
    python_paths(dir, sizeof(dir), exe, sizeof(exe), scripts, sizeof(scripts));
    if (!file_exists(exe)) return;

#ifdef _WIN32
    char current[32767] = {0};
    DWORD n = GetEnvironmentVariableA("Path", current, sizeof(current));
    if (n == 0 || n >= sizeof(current)) current[0] = '\0';
    if (strstr(current, dir) && strstr(current, scripts)) return;

    char updated[32768];
    snprintf(updated, sizeof(updated), "%s;%s;%s", dir, scripts, current);
    SetEnvironmentVariableA("Path", updated);
#else
    const char *current = getenv("PATH");
    if (current && strstr(current, scripts)) return;

    char updated[32768];
    snprintf(updated, sizeof(updated), "%s%s%s", scripts,
             (current && current[0]) ? ":" : "", current ? current : "");
    setenv("PATH", updated, 1);
#endif
}

#ifdef _WIN32
static void python_bootstrap_pip(const char *exe) {
    char *tmp = get_temp_dir();
    char getpip[1024];
    snprintf(getpip, sizeof(getpip), "%s%cget-pip.py", tmp, PATH_SEP);

    if (!download_file(GET_PIP_URL, getpip)) return;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "\"%s\" \"%s\" --no-warn-script-location --disable-pip-version-check > nul 2>&1",
        exe, getpip);
    exec_cmd_silent(cmd);
    remove(getpip);
}
#endif

bool runtime_ensure_python(void) {
    char dir[1024], exe[1024], scripts[1024];
    python_paths(dir, sizeof(dir), exe, sizeof(exe), scripts, sizeof(scripts));

    if (file_exists(exe)) {
        runtime_apply_python_path();
#ifdef _WIN32
        char pip[1024];
        snprintf(pip, sizeof(pip), "%s%cpip.exe", scripts, PATH_SEP);
        if (!file_exists(pip)) python_bootstrap_pip(exe);
#endif
        return true;
    }

    printf("  %s\n", retro("Setting up local Python (isolated)..."));

#ifdef _WIN32
    char *tmp = get_temp_dir();
    char zip_path[1024];
    snprintf(zip_path, sizeof(zip_path), "%s%cpython-embed.zip", tmp, PATH_SEP);

    if (!download_file(PYTHON_EMBED_URL, zip_path)) {
        printf("  %s\n", retro_warn("Local Python download failed; CLI continues without it."));
        return false;
    }

    if (dir_exists(dir)) dir_remove_recursive(dir);
    dir_create(dir);

    char esc_zip[2048], esc_dir[2048];
    ps_escape(zip_path, esc_zip, sizeof(esc_zip));
    ps_escape(dir, esc_dir, sizeof(esc_dir));

    char extract_cmd[4096];
    snprintf(extract_cmd, sizeof(extract_cmd),
        "powershell -NoProfile -Command \"Expand-Archive -LiteralPath '%s' -DestinationPath '%s' -Force\"",
        esc_zip, esc_dir);

    if (!exec_cmd_silent(extract_cmd)) {
        printf("  %s\n", retro_warn("Could not extract local Python; CLI continues without it."));
        remove(zip_path);
        return false;
    }
    remove(zip_path);

    /* The embeddable build disables site-packages by default. */
    char pth[1024];
    snprintf(pth, sizeof(pth), "%s%cpython312._pth", dir, PATH_SEP);
    if (file_exists(pth)) {
        char *content = read_file(pth);
        if (content) {
            char updated[4096];
            size_t o = 0;
            const char *s = content;
            while (*s && o < sizeof(updated) - 1) {
                if (strncmp(s, "#import site", 12) == 0) {
                    const char *repl = "import site";
                    size_t rl = strlen(repl);
                    if (o + rl >= sizeof(updated)) break;
                    memcpy(updated + o, repl, rl);
                    o += rl;
                    s += 12;
                } else {
                    updated[o++] = *s++;
                }
            }
            updated[o] = '\0';
            if (!strstr(updated, "import site") && o + 13 < sizeof(updated)) {
                memcpy(updated + o, "\nimport site\n", 13);
                o += 13;
                updated[o] = '\0';
            }
            write_file(pth, updated);
            free(content);
        }
    }

    python_bootstrap_pip(exe);
#else
    if (!is_in_path("python3")) {
        printf("  %s\n", retro_warn("python3 not found; local Python was not installed."));
        return false;
    }

    if (dir_exists(dir)) dir_remove_recursive(dir);

    char venv_cmd[2048];
    snprintf(venv_cmd, sizeof(venv_cmd), "python3 -m venv \"%s\"", dir);
    if (system(venv_cmd) != 0) {
        printf("  %s\n", retro_warn("Could not create local Python venv; CLI continues without it."));
        return false;
    }
#endif

    if (!file_exists(exe)) {
        printf("  %s\n", retro_warn("Local Python install incomplete; CLI continues without it."));
        return false;
    }

    runtime_apply_python_path();

    char marker[1024];
    snprintf(marker, sizeof(marker), "%s%c.emtypyie-python-version", dir, PATH_SEP);
    write_file(marker, PYTHON_VERSION "\n");

    printf("  %s %s\n", retro("Local Python ready:"), retro_dim(dir));
    return true;
}
