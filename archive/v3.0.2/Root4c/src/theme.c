#include "theme.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define ANSI_BUF_POOL 12
static char ansi_bufs[ANSI_BUF_POOL][4096];
static int ansi_idx = 0;

/* ─── Terminal theming module ───
 * Provides ANSI 24-bit color support via retro_*() functions.
 * Themes are defined as hex color palettes. To add a new theme:
 *   1. Add an entry to THEMES[] below with name + 5 hex colors.
 *   2. Increment THEME_COUNT (or remove the hardcoded count).
 *
 * Theme color order: main, dim, accent, warn, err.
 *
 * Future work:
 *  - User-defined custom themes (read from JSON file).
 *  - True 24-bit color detection (COLORTERM env var).
 *  - Background/border color support.
 */

static const Theme THEMES[] = {
    {"slate",  "#e2e8f0", "#64748b", "#38bdf8", "#fbbf24", "#f87171"},
    {"green",  "#33ff33", "#1a7a1a", "#66ff66", "#ffaa00", "#ff3333"},
    {"amber",  "#ffb000", "#805800", "#ffd700", "#ff6600", "#ff2200"},
    {"violet", "#c084fc", "#7c3aed", "#a78bfa", "#fbbf24", "#f87171"},
    {"cyan",   "#22d3ee", "#0891b2", "#67e8f9", "#fbbf24", "#f87171"},
};
static const int THEME_COUNT = 5;
static int current_idx = 0;

Theme g_theme;

static void enable_ansi_on_windows(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    if (GetConsoleMode(hOut, &mode)) {
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

static void apply_theme(int idx) {
    if (idx < 0 || idx >= THEME_COUNT) idx = 0;
    current_idx = idx;
    g_theme = THEMES[idx];
}

void theme_init(const char *name) {
    enable_ansi_on_windows();
    for (int i = 0; i < THEME_COUNT; i++) {
        if (strcmp(THEMES[i].name, name) == 0) {
            apply_theme(i);
            return;
        }
    }
    apply_theme(3);
}

const char* theme_list(void) {
    static char buf[128];
    buf[0] = '\0';
    for (int i = 0; i < THEME_COUNT; i++) {
        if (i > 0) strcat(buf, ", ");
        strcat(buf, THEMES[i].name);
    }
    return buf;
}

const char* theme_current(void) {
    return THEMES[current_idx].name;
}

static const char* ansi(const char *hex, const char *text) {
    char *buf = ansi_bufs[ansi_idx];
    ansi_idx = (ansi_idx + 1) % ANSI_BUF_POOL;
    int r, g, b;
    sscanf(hex, "#%02x%02x%02x", &r, &g, &b);
    snprintf(buf, 4096, "\033[38;2;%d;%d;%dm%s\033[0m", r, g, b, text);
    return buf;
}

const char* retro(const char *text)          { return ansi(g_theme.main, text); }
const char* retro_dim(const char *text)      { return ansi(g_theme.dim, text); }
const char* retro_accent(const char *text)   { return ansi(g_theme.accent, text); }
const char* retro_warn(const char *text)     { return ansi(g_theme.warn, text); }
const char* retro_err(const char *text)      { return ansi(g_theme.err, text); }
