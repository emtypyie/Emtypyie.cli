#ifndef UTIL_H
#define UTIL_H

/* ─── Cross-platform utilities ───
 * File system helpers, directory management, command execution.
 * All functions work on both Windows and Linux/macOS.
 *
 * Future: add path_normalize(), read_file_lines(), glob_match().
 */

#include <stdbool.h>

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

/* Join two path components with the platform separator. Caller must free(). */
char* path_join(const char *a, const char *b);

bool file_exists(const char *path);
bool dir_exists(const char *path);
bool dir_create(const char *path);
bool dir_remove(const char *path);

/* Recursive delete — uses Shell API on Windows, rm -rf on POSIX. */
bool dir_remove_recursive(const char *path);

/* Reads entire file into malloc'd buffer, or returns NULL on error. Caller must free(). */
char* read_file(const char *path);

/* Writes string content to file, returns true on success. */
bool write_file(const char *path, const char *content);

char* get_home_dir(void);
char* get_emty_dir(void);        /* ~/.emtypyie/ */
char* get_dev_dir(const char *name);  /* ~/.emtypyie/dev/<name>/ */
char* get_runtimes_dir(void);    /* ~/.emtypyie/runtimes/ */
char* get_temp_dir(void);

/* Execute command and capture stdout+stderr into out buffer. Returns exit code. */
int  exec_cmd(const char *cmd, char *out, int out_size);

/* Execute command silently, returns true if exit code == 0. */
bool exec_cmd_silent(const char *cmd);

/* Returns true if the executable is found in PATH. */
bool is_in_path(const char *name);

/* Permanently add a directory to the user PATH (Windows only). */
void add_to_user_path(const char *dir);

/* Platform check — compile-time constant. */
bool is_windows(void);

/* Spawn a process detached from the CLI (no wait, no console window).
 * On Windows uses CreateProcess with DETACHED_PROCESS.
 * On POSIX uses fork() + exec().
 * Returns true if the process was launched successfully. */
bool spawn_detached(const char *cmd);

#endif
