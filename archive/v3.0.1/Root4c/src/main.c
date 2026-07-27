#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "theme.h"
#include "shell.h"
#include "util.h"
#include "effects.h"
#include "fetch.h"
#include "project.h"
#include "util.h"

/* ─── JSON bridge mode (for the Electron GUI) ───
 * When --json is passed, commands emit a single line JSON object:
 *   {"cmd":"...","ok":true|false,"msg":"...","data":{...}}
 * instead of human-formatted text. Keeps the GUI <-> C contract stable.
 */

static int g_json = 0;

static void json_emit_str(const char *cmd, int ok, const char *msg) {
    printf("{\"cmd\":\"");
    fputs(cmd, stdout);
    printf("\",\"ok\":%s", ok ? "true" : "false");
    if (msg && *msg) {
        printf(",\"msg\":\"");
        for (const char *p = msg; *p; p++) {
            if (*p == '"' || *p == '\\') putchar('\\');
            putchar(*p);
        }
        putchar('"');
    }
    printf("}\n");
    fflush(stdout);
}

/* ─── Startup animation ───
 * Embedded directly in main.c so it needs no external JSON file.
 * Skippable via --no-animation flag or EMTYPYIE_NO_ANIM env var.
 */

static const char *ANIM_LINES[] = {
    "[  OK  ] Initializing emtypyie-core v3.0.1 (Baking Bread)",
    "[  OK  ] Architecture: x86_64-windows-nt",
    "[  OK  ] Memory arena allocated: 2.1 GiB / 16 GiB available",
    "[  OK  ] Page table initialized: 4KB pages, 52-bit virtual addressing",
    "[  OK  ] CPU features detected: AVX2, FMA, SSE4.2, POPCNT, BMI1/2",
    "[  OK  ] High-resolution timer: TSC @ 3.4 GHz calibrated",
    "[  OK  ] Filesystem: NTFS 3.1 mounted on C:\\ (1.8TB/2.0TB free)",
    "[  OK  ] Console: VT100 virtual terminal sequences enabled",
    "[  OK  ] Dependency resolver: topological sort, cycle detection",
    "[  OK  ] Loading module: core/runtime (compiler detector)",
    "[  OK  ] GCC toolchain: gcc.exe 13.2.0 (mingw64) found in PATH",
    "[  OK  ] Theme preference: violet (saved 2026-07-17)",
    "[  OK  ] Emtypyie CLI v3.0.1 ready -- type /help for commands",
    "[  OK  ] Welcome back. The witches are wandering.",
    "[  OK  ] ████████████████████████████████████████ 100%"
};

#define ANIM_LINE_COUNT (sizeof(ANIM_LINES) / sizeof(ANIM_LINES[0]))
#define ANIM_TARGET_COUNT ANIM_LINE_COUNT
#define ANIM_LINE_DELAY_MS 110

/* /about: the braille art (no EMTYPYIE.CLI block). */
static const char *BREAD_ART =
"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣔⣶⣎⠉⠉⠓⠦⡀⠀⠀⠀\n"
"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠴⠋⠁⠀⠀⠀⠙⠻⢆⠀⠀⠀⣹⡄⠀\n"
"⠀⠀⠀⠀⠀⠀⠀⢀⠴⠿⣤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⡇⠀\n"
"⠀⠀⠀⠀⠀⡠⠊⠀⠀⠀⠀⠙⢷⡄⠀⠀⠀⠀⠀⠀⠀⢀⢾⠟⠀⠀\n"
"⠀⠀⠀⠀⡜⠀⠀⠀⠀⠀⠀⠀⠀⠀⠃⠀⠀⠀⠀⡀⣔⣿⠗⠀⠀⠀\n"
"⠀⠀⠀⡮⣝⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡸⣷⡟⠁⠀⠀⠀⠀\n"
"⠀⠀⡞⠀⠀⠁⠙⠀⠀⠀⠀⠀⠀⠀⠀⣠⢀⡯⡞⠁⠀⠀⠀⠀⠀⠀\n"
"⠀⢸⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⢔⡱⡽⢍⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
"⠀⢸⡀⠀⠀⠀⠀⡠⡠⠦⢧⢯⣫⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
"⠀⠀⠙⢷⣧⠭⢝⡻⣿⠿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
"⠀⠀⠀⠀⠀⠉⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀ \n";

static void enable_vt_mode(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(hOut, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, mode);
    }
}

static int get_terminal_width(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80;
}

static void print_line_padded(const char *line, int target_width) {
    int len = (int)strlen(line);
    if (len >= target_width) {
        printf("%.*s\n", target_width, line);
    } else {
        printf("%s%*s\n", line, target_width - len, "");
    }
}

static void startup_animation_play(void) {
    enable_vt_mode();

    int term_width = get_terminal_width();
    int target_width = (int)(term_width * 0.65);
    if (target_width < 40) target_width = 40;
    if (target_width > term_width) target_width = term_width;

    for (int i = 0; i < (int)ANIM_TARGET_COUNT; i++) {
        const char *line = ANIM_LINES[i % ANIM_LINE_COUNT];
        print_line_padded(line, target_width);
        fflush(stdout);
        int delay = ANIM_LINE_DELAY_MS;
        if (i == ANIM_LINE_COUNT / 2) delay = ANIM_LINE_DELAY_MS * 4;
        if (delay > 0) Sleep((DWORD)delay);
    }
}

/* ─── Main entry point ───
 * Parses command-line arguments, loads saved theme, dispatches to handlers.
 * To add a new command: add an else-if block in main() with the command
 * string and call the corresponding handler (declared extern or in a header).
 */

static void print_usage(void) {
    printf("Usage: emtypyie [command] [args]\n");
    printf("\n");
    printf("Commands:\n");
    printf("  /help             Show help\n");
    printf("  /about            About emtypyie\n");
    printf("  /list             List available projects\n");
    printf("  /get    <project> Install a project\n");
    printf("  /get    gcc       Install GCC/G++ compiler\n");
    printf("  /info   <project> Show project details\n");
    printf("  /flash  <project> Re-download latest version\n");
    printf("  /rm     <project> Remove project\n");
    printf("  /run    <project> Launch/run a project\n");
    printf("  /theme  <name>    Change theme\n");
    printf("  /docs   <project> Open documentation\n");
    printf("  /bf               System info (bakafetch)\n");
    printf("  /larpino <cmd>    LLM chat (enable|disable|status)\n");
    printf("  /clear            Clear screen\n");
    printf("  /shell            Interactive mode\n");
    printf("\n");
    printf("Run without arguments to enter interactive mode.\n");
}

int main(int argc, char *argv[]) {
    const char *theme_name = "violet";
    static char saved_theme[512] = {0};
    char *emty_dir = get_emty_dir();

    char theme_path[1024];
    snprintf(theme_path, sizeof(theme_path), "%s%ctheme.txt", emty_dir, PATH_SEP);
    if (file_exists(theme_path)) {
        char *loaded = read_file(theme_path);
        if (loaded) {
            char *nl = strchr(loaded, '\n');
            if (nl) *nl = '\0';
            strncpy(saved_theme, loaded, sizeof(saved_theme) - 1);
            free(loaded);
            theme_name = saved_theme;
        }
    }

    theme_init(theme_name);

    int skip_anim = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-animation") == 0) skip_anim = 1;
        else if (strcmp(argv[i], "--json") == 0) g_json = 1;
    }

    if (argc == 1) {
        if (!skip_anim && !getenv("EMTYPYIE_NO_ANIM")) {
            startup_animation_play();
        }
        shell_run();
        return 0;
    }

    /* Normalize the command: accept it with or without a leading slash so the
       GUI bridge can call "about" or "/about" interchangeably. */
    static char cmd_buf[512];
    const char *raw = argv[1];
    if (raw[0] == '/') {
        strncpy(cmd_buf, raw, sizeof(cmd_buf) - 1);
        cmd_buf[sizeof(cmd_buf) - 1] = '\0';
    } else {
        cmd_buf[0] = '/';
        strncpy(cmd_buf + 1, raw, sizeof(cmd_buf) - 2);
        cmd_buf[sizeof(cmd_buf) - 1] = '\0';
    }
    const char *cmd = cmd_buf;

    /* Argument = first non-flag token after argv[1] (so order of --json vs
       arg in the GUI bridge does not matter). */
    const char *arg = "";
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0 || strcmp(argv[i], "--no-animation") == 0) continue;
        arg = argv[i];
        break;
    }

    if      (strcmp(cmd, "/help") == 0) {
        print_usage();
        if (g_json) json_emit_str(cmd, 1, "Usage printed.");
    }
    else if (strcmp(cmd, "/about") == 0) {
        if (g_json) {
            json_emit_str(cmd, 1, "EMTYPYIE CLI v3.0.1 \"Baking Bread\"");
        } else {
            printf("  EMTYPYIE.CLI v3.0.1\n\n");
            printf("%s\n\n", BREAD_ART);
            printf("Release: Baking Bread\n");
            printf("Engineered And Designed by EMTYPYIE\n");
            printf("copyright EMTYPYIE, All Rights Reserved\n");
        }
    }
    else if (strcmp(cmd, "/get") == 0) {
        if (strlen(arg) == 0) {
            if (g_json) json_emit_str(cmd, 0, "Specify a project.");
            else printf("  Specify a project.\n");
            return 1;
        }
        if (strcmp(arg, "gcc") == 0 || strcmp(arg, "g++") == 0) {
            extern void runtime_install_compiler(const char*);
            runtime_install_compiler(arg);
        } else {
            extern void project_get(const char*);
            if (!g_json) project_get(arg);
        }
        if (g_json) json_emit_str(cmd, 1, arg);
    }
    else if (strcmp(cmd, "/info") == 0) {
        if (strlen(arg) == 0) {
            if (g_json) json_emit_str(cmd, 0, "Specify a project.");
            else printf("  Specify a project.\n");
            return 1;
        }
        extern void project_info(const char*);
        if (!g_json) project_info(arg);
        if (g_json) json_emit_str(cmd, 1, arg);
    }
    else if (strcmp(cmd, "/list") == 0) {
        extern void project_list(void);
        if (!g_json) project_list();
        if (g_json) json_emit_str(cmd, 1, "Listed.");
    }
    else if (strcmp(cmd, "/flash") == 0) {
        if (strlen(arg) == 0) {
            if (g_json) json_emit_str(cmd, 0, "Specify a project.");
            else printf("  Specify a project.\n");
            return 1;
        }
        extern void project_flash(const char*);
        if (!g_json) project_flash(arg);
        if (g_json) json_emit_str(cmd, 1, arg);
    }
    else if (strcmp(cmd, "/rm") == 0) {
        if (strlen(arg) == 0) {
            if (g_json) json_emit_str(cmd, 0, "Specify a project.");
            else printf("  Specify a project.\n");
            return 1;
        }
        extern void project_remove(const char*);
        if (!g_json) project_remove(arg);
        if (g_json) json_emit_str(cmd, 1, arg);
    }
    else if (strcmp(cmd, "/theme") == 0) {
        if (strlen(arg) == 0) {
            if (g_json) json_emit_str(cmd, 1, theme_current());
            else {
                printf("  Current theme: %s\n", theme_current());
                printf("  Available: %s\n", theme_list());
            }
        } else {
            theme_init(arg);
            char theme_path[1024];
            snprintf(theme_path, sizeof(theme_path), "%s%ctheme.txt", get_emty_dir(), PATH_SEP);
            write_file(theme_path, arg);
            if (g_json) json_emit_str(cmd, 1, theme_current());
            else { printf("  Theme set to %s\n", retro(theme_current())); }
        }
    }
    else if (strcmp(cmd, "/docs") == 0) {
        if (strlen(arg) == 0) {
            if (g_json) json_emit_str(cmd, 0, "Specify a project.");
            else printf("  Specify a project.\n");
            return 1;
        }
        extern void project_docs(const char*);
        if (!g_json) project_docs(arg);
        if (g_json) json_emit_str(cmd, 1, arg);
    }
    else if (strcmp(cmd, "/bf") == 0 || strcmp(cmd, "/bakafetch") == 0) {
        extern void bakafetch_show(void);
        if (!g_json) bakafetch_show();
        if (g_json) json_emit_str(cmd, 1, "System info printed.");
    }
    else if (strcmp(cmd, "/larpino") == 0) {
        if (g_json) json_emit_str(cmd, 0, "Use /larpino from the interactive shell.");
        else printf("  %s\n", retro_dim("Use /larpino from the interactive shell."));
    }
    else if (strcmp(cmd, "/clear") == 0) {
        if (!g_json) printf("\033[2J\033[H");
    }
    else if (strcmp(cmd, "/shell") == 0) {
        shell_run();
    }
    else if (strcmp(cmd, "/issue") == 0) {
        if (strlen(arg) == 0) {
            if (g_json) json_emit_str(cmd, 0, "Specify a project.");
            else printf("  Specify a project.\n");
            return 1;
        }
        if (!g_json) {
            char url[512];
            snprintf(url, sizeof(url), "https://cdn.emtypyie.in/dev/%s/metadata.json", arg);
            FetchResult *ir = fetch_get_with_timeout(url, 5);
            if (!ir || ir->status_code != 200) {
                printf("  %s %s\n", retro_err("Could not fetch project info:"), retro_dim(arg));
                if (ir) fetch_free(ir);
                return 1;
            }
            char *repo = json_string(ir->body, "\"repo\":");
            if (repo) {
                char issue_url[512];
                snprintf(issue_url, sizeof(issue_url), "https://github.com/%s/issues", repo);
                printf("  %s %s\n", retro("Opening issues for"), retro_accent(arg));
                char icmd[1024]; snprintf(icmd, sizeof(icmd), "start %s", issue_url); system(icmd);
                free(repo);
            }
            fetch_free(ir);
        }
        if (g_json) json_emit_str(cmd, 1, arg);
    }
    else if (strcmp(cmd, "/setenv") == 0) {
        extern void auth_setenv(void);
        auth_setenv();
        if (g_json) json_emit_str(cmd, 1, "Environment set.");
    }
    else if (strcmp(cmd, "/update") == 0) {
        extern bool self_update(void);
        int ok = self_update();
        if (g_json) json_emit_str(cmd, ok, ok ? "Updated." : "Update failed or not needed.");
    }
    else if (strcmp(cmd, "/run") == 0) {
        extern bool project_run(const char *);
        if (strlen(arg) == 0) {
            if (g_json) json_emit_str(cmd, 0, "Specify a project.");
            else printf("  Specify a project.\n");
            return 1;
        }
        project_run(arg);
    }
    else {
        extern bool project_run(const char *);
        /* Unknown command: try launching it as a project name (e.g. /qrkraft). */
        project_run(cmd + 1);
    }

    return 0;
}
