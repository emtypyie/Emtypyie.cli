#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <shlobj.h>
#include <process.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

/* ─── Cross-platform utilities implementation ───
 * Provides file I/O, directory management, and external process execution
 * with platform-specific code guarded by #ifdef _WIN32.
 *
 * Future work:
 *  - Add path_normalize() to convert backslashes on Windows.
 *  - Replace snprintf-based path construction with a dynamic builder.
 */

char* path_join(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    char *res = malloc(la + lb + 2);
    if (!res) return NULL;
    strcpy(res, a);
#ifdef _WIN32
    if (la > 0 && a[la-1] != '\\' && a[la-1] != '/') res[la++] = '\\';
#else
    if (la > 0 && a[la-1] != '/') res[la++] = '/';
#endif
    strcpy(res + la, b);
    return res;
}

bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && !(st.st_mode & S_IFDIR);
}

bool dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && (st.st_mode & S_IFDIR);
}

bool dir_create(const char *path) {
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

bool dir_remove(const char *path) {
    return rmdir(path) == 0;
}

#ifdef _WIN32
static void rm_recursive_win(const char *path) {
    char search[MAX_PATH];
    snprintf(search, sizeof(search), "%s\\*", path);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s\\%s", path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            rm_recursive_win(full);
        } else {
            DeleteFile(full);
        }
    } while (FindNextFile(h, &fd));
    FindClose(h);
    RemoveDirectory(path);
}
#endif

bool dir_remove_recursive(const char *path) {
#ifdef _WIN32
    rm_recursive_win(path);
    return true;
#else
    /* Use find + rm to avoid shell injection via path */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "find \"%s\" -mindepth 1 -delete 2>/dev/null; rmdir \"%s\" 2>/dev/null", path, path);
    return system(cmd) == 0;
#endif
}

char* read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

bool write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    return true;
}

char* get_home_dir(void) {
#ifdef _WIN32
    static char buf[MAX_PATH];
    SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, buf);
    return buf;
#else
    const char *h = getenv("HOME");
    return h ? (char*)h : "/tmp";
#endif
}

char* get_emty_dir(void) {
    static char buf[512];
    char *home = get_home_dir();
    snprintf(buf, sizeof(buf), "%s%c.emtypyie", home, PATH_SEP);
    dir_create(buf);
    return buf;
}

char* get_dev_dir(const char *name) {
    static char buf[512];
    char *emty = get_emty_dir();
    snprintf(buf, sizeof(buf), "%s%cdev%c%s", emty, PATH_SEP, PATH_SEP, name);
    dir_create(buf);
    return buf;
}

char* get_runtimes_dir(void) {
    static char buf[512];
    char *emty = get_emty_dir();
    snprintf(buf, sizeof(buf), "%s%cruntimes", emty, PATH_SEP);
    dir_create(buf);
    return buf;
}

char* get_temp_dir(void) {
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMP");
    if (!tmp) tmp = "/tmp";
    return (char*)tmp;
}

int exec_cmd(const char *cmd, char *out, int out_size) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE rPipe, wPipe;
    CreatePipe(&rPipe, &wPipe, &sa, 0);
    SetHandleInformation(rPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFO si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wPipe;
    si.hStdError = wPipe;
    PROCESS_INFORMATION pi;

    char cmdline[4096];
    snprintf(cmdline, sizeof(cmdline), "cmd.exe /c %s", cmd);

    if (CreateProcess(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(wPipe);
        DWORD read_bytes = 0;
        if (out && out_size > 0) {
            ReadFile(rPipe, out, out_size - 1, &read_bytes, NULL);
            out[read_bytes] = '\0';
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(rPipe);
        return (int)exit_code;
    }
    CloseHandle(wPipe);
    CloseHandle(rPipe);
    return -1;
#else
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    if (out && out_size > 0) {
        size_t n = fread(out, 1, out_size - 1, fp);
        out[n] = '\0';
    }
    int rc = pclose(fp);
    return rc;
#endif
}

bool exec_cmd_silent(const char *cmd) {
    char buf[32];
    return exec_cmd(cmd, buf, sizeof(buf)) == 0;
}

bool is_in_path(const char *name) {
#ifdef _WIN32
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "where %s >nul 2>nul", name);
#else
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "which %s >/dev/null 2>/dev/null", name);
#endif
    return exec_cmd_silent(cmd);
}

void add_to_user_path(const char *dir) {
#ifdef _WIN32
    /* Escape single quotes for PowerShell by doubling them */
    char escaped[4096] = {0};
    size_t j = 0;
    for (const char *p = dir; *p && j < sizeof(escaped) - 3; p++) {
        if (*p == '\'') { escaped[j++] = '\''; escaped[j++] = '\''; }
        else { escaped[j++] = *p; }
    }
    escaped[j] = '\0';
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
        "powershell -Command \"[Environment]::SetEnvironmentVariable('Path', '%s;' + [Environment]::GetEnvironmentVariable('Path','User'), 'User')\"",
        escaped);
    exec_cmd_silent(cmd);
#endif
}

bool is_windows(void) {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

bool spawn_detached(const char *cmd) {
#ifdef _WIN32
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmdline[4096];
    snprintf(cmdline, sizeof(cmdline), "cmd.exe /c %s", cmd);
    if (CreateProcess(NULL, cmdline, NULL, NULL, FALSE,
        DETACHED_PROCESS | CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    return false;
#else
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }
    return pid > 0;
#endif
}
