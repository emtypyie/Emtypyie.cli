#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdbool.h>

void runtime_install_compiler(const char *name);

/* Install (or verify) the isolated local Python runtime.
 * Windows: Python embeddable package under ~/.emtypyie/runtimes/python.
 * POSIX:   a local venv at the same location.
 * Returns true when the local interpreter is ready. */
bool runtime_ensure_python(void);

/* Prepend the local Python directories to this process's PATH. */
void runtime_apply_python_path(void);

/* Absolute path to the local Python interpreter (empty if not installed). */
const char *runtime_python_exe(void);

/* True when the local interpreter exists. */
bool runtime_python_installed(void);

#endif
