#include "shell.h"
#include "theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include "util.h"
#include "fetch.h"
#include "project.h"
#include "bakafetch.h"
#include "runtime.h"
#include "download.h"
#include "auth.h"
#include "larpino.h"
#include "effects.h"

#define MAX_HISTORY 100

static larpino_model *g_larpino = NULL;
static int g_larpino_chat = 0;

static void larpino_print_token(const char *tok, void *user) {
    (void)user;
    printf("%s", tok);
    fflush(stdout);
}
#define MAX_LINE 4096
#define MAX_COMMANDS 64

static const char* COMMANDS[] = {
    "help", "about", "wiki", "setenv", "theme", "info", "get", "flash", "rm",
    "issue", "bakafetch", "bf", "wrap", "clear", "update", "list", "docs",
    "changelog", "larpino", "exit", "quit", NULL
};

static char history[MAX_HISTORY][MAX_LINE];
static int hist_count = 0;
static int hist_pos = 0;


static const char BANNER[] =
"___________        __                         .__                  .__  .__\n"
"\\_   _____/ ______/  |_ ___.__. ______ ___.__.|__| ____       ____ |  | |__|\n"
" |    __)_ /     \\   __<   |  | \\____ <   |  ||  |/ __ \\    _/ ___\n"
" |        \\  Y Y  \\  |  \\___  | |  |_> >___  ||  \\  ___/    \\  \\___|  |_|  |\n"
"/_______  /__|_|  /__|  / ____| |   __// ____||__|\\___  > /\\ \\___  >____/__|\n"
"  EMTYPYIE CLI v3.0.2\n"
"  \"Baking Bread\"\n"
"\n"
"  ---------------------------------------------\n"
"  \"code. create. conquer.\"\n"
"  ---------------------------------------------\n"
"\n"
"  Type /help for available commands.\n"
"  ---------------------------------------------\n";

/* /about: the braille art (no EMTYPYIE.CLI block). */
static const char BREAD_ART[] =
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

static void add_history(const char *line) {
    if (hist_count < MAX_HISTORY) {
        strncpy(history[hist_count], line, MAX_LINE - 1);
        hist_count++;
    } else {
        memmove(history, history + 1, (MAX_HISTORY - 1) * MAX_LINE);
        strncpy(history[MAX_HISTORY - 1], line, MAX_LINE - 1);
    }
    hist_pos = hist_count;
}

#define CURRENT_VERSION "v3.0.2"

bool self_update(void) {
    printf("  %s\n", retro("Checking for updates..."));
    FetchResult *r = fetch_get_with_timeout(
        "https://api.github.com/repos/emtypyie/Emtypyie.cli/releases/latest", 15);
    if (!r || r->status_code != 200 || !r->body) {
        printf("  %s\n", retro_err("Could not reach update server"));
        if (r) fetch_free(r);
        return false;
    }

    char ver[32] = {0};
    const char *p = strstr(r->body, "\"tag_name\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p++;
            while (*p == ' ' || *p == '"') p++;
            const char *q = p;
            while (*q && *q != '"') q++;
            size_t n = (size_t)(q - p);
            if (n > 31) n = 31;
            memcpy(ver, p, n);
            ver[n] = '\0';
        }
    }

    if (!ver[0]) {
        printf("  %s\n", retro_err("No release information found"));
        fetch_free(r);
        return false;
    }

    printf("  %s %s\n", retro_dim("Latest:"), retro(ver));

    if (strcmp(ver, CURRENT_VERSION) <= 0) {
        printf("  %s %s\n", retro_dim("Already up to date"), retro_dim(ver));
        fetch_free(r);
        return false;
    }

    const char *asset = strstr(r->body, "emtypyie-cli-native-windows-x64-");
    char url[2048] = {0};
    if (asset) {
        const char *scan = r->body;
        const char *found = NULL;
        while ((scan = strstr(scan, "browser_download_url")) != NULL) {
            if (scan < asset && (asset - scan) < 512) { found = scan; break; }
            scan += strlen("browser_download_url");
        }
        if (found) {
            const char *q = strchr(found, '"');
            if (q) {
                q++;
                const char *s = strchr(q, '"');
                if (s) {
                    size_t n = (size_t)(s - q);
                    if (n > 2047) n = 2047;
                    memcpy(url, q, n);
                    url[n] = '\0';
                }
            }
        }
    }
    fetch_free(r);

    if (!url[0]) {
        printf("  %s\n", retro_err("No native asset found in latest release"));
        return false;
    }

    char extract_dir[2048];
    snprintf(extract_dir, sizeof(extract_dir), "%s%c_emtypyie_update", get_temp_dir(), PATH_SEP);
    dir_create(extract_dir);

    if (!download_zip_and_extract(url, extract_dir)) {
        printf("  %s\n", retro_err("Update download failed"));
        dir_remove_recursive(extract_dir);
        return false;
    }

    char new_exe[3072];
    snprintf(new_exe, sizeof(new_exe), "%s%cemtypyie.exe", extract_dir, PATH_SEP);
    if (!file_exists(new_exe)) {
        printf("  %s\n", retro_err("Updated executable not found"));
        dir_remove_recursive(extract_dir);
        return false;
    }

    /* Basic integrity check: a Windows exe must start with the "MZ" DOS header. */
    bool ok = false;
    {
        FILE *f = fopen(new_exe, "rb");
        if (f) {
            unsigned char hdr[2] = {0, 0};
            if (fread(hdr, 1, 2, f) == 2 && hdr[0] == 'M' && hdr[1] == 'Z')
                ok = true;
            fclose(f);
        }
    }
    if (!ok) {
        printf("  %s\n", retro_err("Downloaded update is not a valid executable"));
        dir_remove_recursive(extract_dir);
        return false;
    }

    /* Replace the running binary with the new one so the update persists,
       then re-exec our own (now updated) path. */
    char self_path[MAX_PATH] = {0};
    if (GetModuleFileName(NULL, self_path, MAX_PATH) == 0) self_path[0] = '\0';

    if (self_path[0] && strcmp(self_path, new_exe) != 0) {
        /* Try to overwrite ourselves; keep the extracted copy as fallback. */
        if (!CopyFileA(new_exe, self_path, FALSE)) {
            printf("  %s\n", retro_dim("Could not replace running binary; launching update directly."));
        } else {
            printf("  %s\n", retro_dim("Installed update to current location."));
        }
    }

    printf("  %s %s\n", retro("Restarting with"), retro(ver));
    fflush(stdout);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "\"%s\"", self_path[0] ? self_path : new_exe);
    dir_remove_recursive(extract_dir);
    spawn_detached(cmd);
    exit(0);
}

static void rtrim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n-1])) s[--n] = '\0';
}

static void handle_command(const char *input) {
    char cmd[256] = {0}, arg[2048] = {0};
    const char *p = input;
    if (*p == '/') p++;
    while (*p && !isspace(*p) && strlen(cmd) < 255) { cmd[strlen(cmd)] = *p; p++; }
    while (*p && isspace(*p)) p++;
    strncpy(arg, p, 2047);
    rtrim(arg);

    if (strcmp(cmd, "help") == 0) {
        printf("\n");
        printf("  %s\n", retro_dim("─── Commands ───"));
        printf("\n");
        printf("  %s  %s\n", retro("/get <project>"), retro_dim("   install a project"));
        printf("  %s  %s\n", retro("/get gcc"), retro_dim("         auto-install GCC/G++ compiler"));
        printf("  %s  %s\n", retro("/flash <project>"), retro_dim("  re-download latest version"));
        printf("  %s  %s\n", retro("/info <project>"), retro_dim("   show project details"));
        printf("  %s  %s\n", retro("/rm <project>"), retro_dim("     delete project files"));
        printf("  %s  %s\n", retro("/issue <project>"), retro_dim("  open issue tracker"));
        printf("  %s  %s\n", retro("/setenv"), retro_dim("          set env variables"));
        printf("  %s  %s\n", retro("/theme <name>"), retro_dim("      change CLI color theme"));
        printf("  %s  %s\n", retro("/bakafetch"), retro_dim("        system info with style"));
        printf("  %s  %s\n", retro("/bf"), retro_dim("              shortcut for /bakafetch"));
        printf("  %s  %s\n", retro("/clear"), retro_dim("             clear screen"));
        printf("  %s  %s\n", retro("/update"), retro_dim("           update emtypyie"));
        printf("  %s  %s\n", retro("/list"), retro_dim("             list projects"));
        printf("  %s  %s\n", retro("/docs <project>"), retro_dim("   open project docs"));
        printf("  %s  %s\n", retro("/changelog"), retro_dim("        what's new"));
        printf("  %s  %s\n", retro("/about"), retro_dim("            about emtypyie"));
        printf("  %s  %s\n", retro("/wiki"), retro_dim("             open wiki.emtypyie.in"));
        printf("  %s  %s\n", retro("/larpino <cmd>"), retro_dim("     enable|disable|status"));
        printf("  %s  %s\n", retro("/get larpino@1b"), retro_dim("    download 1B LLM model"));
        printf("  %s  %s\n", retro("/help"), retro_dim("            this screen"));
        printf("  %s  %s\n", retro("/exit"), retro_dim("            quit"));
        printf("\n");
        return;
    }

    if (strcmp(cmd, "about") == 0) {
        printf("  EMTYPYIE.CLI v3.0.2\n\n");
        printf("%s\n\n", BREAD_ART);
        printf("Release: Baking Bread\n");
        printf("Engineered And Designed by EMTYPYIE\n");
        printf("copyright EMTYPYIE, All Rights Reserved\n");
        printf("\n");
        return;
    }

    if (strcmp(cmd, "clear") == 0) {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
        return;
    }

    if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        printf("  %s\n", retro_dim("System halted."));
        exit(0);
    }

    if (strcmp(cmd, "wiki") == 0) {
        printf("  %s\n", retro("Opening wiki.emtypyie.in..."));
#ifdef _WIN32
        system("start https://wiki.emtypyie.in");
#else
        system("xdg-open https://wiki.emtypyie.in");
#endif
        return;
    }

    if (strcmp(cmd, "theme") == 0) {
        if (strlen(arg) == 0) {
            printf("  %s %s\n", retro("Usage: /theme <name>"), retro_dim("Themes: "));
            printf("  %s\n", retro(theme_list()));
            return;
        }
        theme_init(arg);
        printf("  %s %s\n", retro("Theme set to"), retro_accent(arg));
        return;
    }

    if (strcmp(cmd, "bakafetch") == 0 || strcmp(cmd, "bf") == 0) {
        bakafetch_show();
        return;
    }

    if (strcmp(cmd, "list") == 0) {
        project_list();
        return;
    }

    if (strcmp(cmd, "get") == 0) {
        if (strlen(arg) == 0) {
            printf("  %s\n", retro_err("Specify a project: /get <project>"));
            return;
        }
        if (strcmp(arg, "gcc") == 0 || strcmp(arg, "g++") == 0) {
            runtime_install_compiler(arg);
        } else {
            /* check for larpino@<variant> syntax */
            const char *at = strchr(arg, '@');
            if (at && strncmp(arg, "larpino", 7) == 0) {
                char variant[64] = {0};
                strncpy(variant, at + 1, sizeof(variant) - 1);
                char url[512];
                snprintf(url, sizeof(url), "https://cdn.emtypyie.in/dev/larpino/%s/model.q4_0.gguf", variant);
                char *dir = get_dev_dir("larpino");
                char dest[1024];
                snprintf(dest, sizeof(dest), "%s\\%s\\model.q4_0.gguf", dir, variant);
                printf("  %s %s\n", retro_dim("Downloading larpino"), retro_accent(variant));
                if (download_file(url, dest)) {
                    printf("  %s\n", retro("Loading model..."));
                    if (g_larpino) larpino_free(g_larpino);
                    g_larpino = larpino_load(dest);
                    if (g_larpino && larpino_is_loaded(g_larpino)) {
                        printf("  %s\n", retro("larpino ready. Use /larpino enable to chat."));
                    } else {
                        printf("  %s\n", retro_err("Failed to load model."));
                    }
                } else {
                    printf("  %s %s\n", retro_err("Download failed:"), retro_dim(variant));
                }
            } else {
                project_get(arg);
            }
        }
        return;
    }

    if (strcmp(cmd, "info") == 0) {
        if (strlen(arg) == 0) {
            printf("  %s\n", retro_err("Specify a project: /info <project>"));
            return;
        }
        project_info(arg);
        return;
    }

    if (strcmp(cmd, "flash") == 0) {
        if (strlen(arg) == 0) {
            printf("  %s\n", retro_err("Specify a project: /flash <project>"));
            return;
        }
        project_flash(arg);
        return;
    }

    if (strcmp(cmd, "rm") == 0) {
        if (strlen(arg) == 0) {
            printf("  %s\n", retro_err("Specify a project: /rm <project>"));
            return;
        }
        project_remove(arg);
        return;
    }

    if (strcmp(cmd, "docs") == 0) {
        if (strlen(arg) == 0) {
            printf("  %s\n", retro_err("Specify a project: /docs <project>"));
            return;
        }
        project_docs(arg);
        return;
    }

    if (strcmp(cmd, "larpino") == 0) {
        if (strcmp(arg, "enable") == 0) {
            if (!g_larpino || !larpino_is_loaded(g_larpino)) {
                printf("  %s\n", retro_err("No model loaded. Use /get larpino@1b first."));
                return;
            }
            g_larpino_chat = 1;
            printf("  %s\n", retro("larpino chat mode enabled. Type anything to chat."));
            printf("  %s\n", retro_dim("Type /larpino disable to exit chat mode."));
            return;
        }
        if (strcmp(arg, "disable") == 0) {
            g_larpino_chat = 0;
            if (g_larpino) larpino_free(g_larpino);
            g_larpino = NULL;
            printf("  %s\n", retro("larpino disabled."));
            return;
        }
        if (strcmp(arg, "status") == 0) {
            char buf[256];
            larpino_status(g_larpino, buf, sizeof(buf));
            printf("  %s  %s\n", retro("larpino:"), retro_dim(buf));
            return;
        }
        printf("  %s\n", retro_dim("Usage: /larpino enable | disable | status"));
        return;
    }

    if (strcmp(cmd, "changelog") == 0) {
        printf("  %s\n", retro("Opening changelog..."));
#ifdef _WIN32
        system("start https://github.com/emtypyie/emtypyie-cli/releases");
#else
        system("xdg-open https://github.com/emtypyie/emtypyie-cli/releases");
#endif
        return;
    }

    if (strcmp(cmd, "issue") == 0) {
        if (strlen(arg) == 0) {
            printf("  %s\n", retro_err("Specify a project: /issue <project>"));
            return;
        }
        char url[4096];
        snprintf(url, sizeof(url), "%s/%s/metadata.json", API_BASE, arg);
        FetchResult *ir = fetch_get_with_timeout(url, 5);
        if (!ir || ir->status_code != 200) {
            printf("  %s %s\n", retro_err("Could not fetch project info:"), retro_dim(arg));
            if (ir) fetch_free(ir);
            return;
        }
        char *repo = json_string(ir->body, "\"repo\":");
        if (repo) {
            char issue_url[512];
            snprintf(issue_url, sizeof(issue_url), "https://github.com/%s/issues", repo);
            printf("  %s %s\n", retro("Opening issues for"), retro_accent(arg));
#ifdef _WIN32
            char icmd[1024]; snprintf(icmd, sizeof(icmd), "start \"\" \"%s\"", issue_url); system(icmd);
#else
            char icmd[1024]; snprintf(icmd, sizeof(icmd), "xdg-open \"%s\"", issue_url); system(icmd);
#endif
            free(repo);
        } else {
            printf("  %s %s\n", retro_err("No repo field for:"), retro_dim(arg));
        }
        fetch_free(ir);
        return;
    }

    if (strcmp(cmd, "wrap") == 0) {
        if (strlen(arg) == 0) {
            printf("  %s\n", retro_err("Specify a project: /wrap <project>"));
            return;
        }
        char *dir = get_dev_dir(arg);
        if (!dir_exists(dir)) {
            printf("  %s %s\n", retro_err("Project not installed:"), retro_dim(arg));
            return;
        }
        printf("  %s %s\n", retro("Project files for"), retro_accent(arg));
        char list_cmd[1024];
#ifdef _WIN32
        snprintf(list_cmd, sizeof(list_cmd), "dir /b \"%s\"", dir);
#else
        snprintf(list_cmd, sizeof(list_cmd), "ls -la \"%s\"", dir);
#endif
        system(list_cmd);
        return;
    }

    if (strcmp(cmd, "update") == 0) {
        self_update();
        return;
    }

    if (strcmp(cmd, "setenv") == 0) {
        auth_setenv();
        return;
    }

    if (!project_run(cmd)) {
        printf("  %s %s\n", retro_err("Unknown command:"), retro_dim(cmd));
        printf("  %s\n", retro_dim("Type /help for available commands."));
    }
}

#ifndef _WIN32
static void enable_raw_mode(void) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void disable_raw_mode(void) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag |= ECHO | ICANON;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
#endif

static int str_prefix(const char *str, const char *prefix) {
    while (*prefix) {
        if (tolower(*str) != tolower(*prefix)) return 0;
        str++; prefix++;
    }
    return 1;
}

static int find_completions(const char *input, char out[MAX_COMMANDS][256]) {
    int count = 0;
    const char *p = input;
    if (*p == '/') p++;
    for (int i = 0; COMMANDS[i] != NULL && count < MAX_COMMANDS; i++) {
        if (str_prefix(COMMANDS[i], p)) {
            strncpy(out[count], COMMANDS[i], 255);
            count++;
        }
    }
    return count;
}

void shell_run(void) {
    int piped = 0;
#ifdef _WIN32
    HANDLE hInCheck = GetStdHandle(STD_INPUT_HANDLE);
    DWORD in_mode_check;
    piped = !GetConsoleMode(hInCheck, &in_mode_check);
#else
    piped = !isatty(fileno(stdin));
#endif

    if (!piped) {
        printf("%s", retro(BANNER));
        printf("  %s\n", retro_accent("EMTYPYIE CLI v3.0.2"));
        printf("  %s\n", retro_dim("\"Baking Bread\""));
        printf("\n");
        printf("  %s\n", retro_dim("─────────────────────────────────────────────"));
        printf("  %s", retro("\"code. create. conquer.\""));
        printf("\n");
        printf("  %s\n", retro_dim("─────────────────────────────────────────────"));
        printf("\n");
        printf("  %s %s %s\n", retro("Type"), retro_accent("/help"), retro_dim("for available commands."));
        printf("  %s\n", retro_dim("─────────────────────────────────────────────"));
        printf("\n");
    }

    char buf[MAX_LINE];
    int pos = 0;
    hist_pos = 0;
    int tab_count = 0;
    int tab_cmd_count = 0;
    char tab_matches[MAX_COMMANDS][256];
    int tab_idx = 0;

    if (piped) {
        printf("__EMT_READY__\n");
        fflush(stdout);
    } else {
        printf(" %s", retro(">> "));
        fflush(stdout);
    }

#ifdef _WIN32
    if (piped) {
        while (fgets(buf, MAX_LINE, stdin)) {
            size_t len = strlen(buf);
            while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
            if (len == 0) { printf("__EMT_READY__\n"); fflush(stdout); continue; }
            if (g_larpino_chat && buf[0] != '/') {
                printf("  larpino :");
                fflush(stdout);
                larpino_chat(g_larpino, buf, larpino_print_token, NULL);
                printf("\n");
            } else {
                add_history(buf);
                handle_command(buf);
            }
            fflush(stdout);
            printf("__EMT_READY__\n");
            fflush(stdout);
        }
        return;
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD old_mode;
    GetConsoleMode(hIn, &old_mode);
    SetConsoleMode(hIn, old_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));

    INPUT_RECORD rec;
    DWORD read_count;
    while (1) {
        ReadConsoleInput(hIn, &rec, 1, &read_count);
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
        KEY_EVENT_RECORD key = rec.Event.KeyEvent;

        if (key.wVirtualKeyCode == VK_RETURN) {
            printf("\n");
            buf[pos] = '\0';
            if (pos > 0) {
                if (g_larpino_chat && buf[0] != '/') {
                    printf("  %s ", retro("larpino :"));
                    fflush(stdout);
                    larpino_chat(g_larpino, buf, larpino_print_token, NULL);
                    printf("\n");
                } else {
                    add_history(buf);
                    handle_command(buf);
                }
            }
            pos = 0;
            tab_count = 0;
            printf(" %s%s", g_larpino_chat ? "" : retro(">> "),
                           g_larpino_chat ? retro_dim("user >> ") : "");
            fflush(stdout);
        } else if (key.wVirtualKeyCode == VK_BACK) {
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
            tab_count = 0;
        } else if (key.wVirtualKeyCode == VK_TAB) {
            buf[pos] = '\0';
            char full_input[256];
            strncpy(full_input, buf, 255);
            int count = find_completions(full_input, tab_matches);
            if (count == 0) {
                tab_count = 0;
            } else if (count == 1) {
                int cmd_len = strlen(tab_matches[0]);
                int in_len = pos;
                if (buf[0] == '/') in_len--;
                for (int i = in_len; i < cmd_len; i++) {
                    buf[pos++] = tab_matches[0][i];
                    printf("%c", buf[pos-1]);
                }
                buf[pos++] = ' ';
                printf(" ");
                fflush(stdout);
                tab_count = 0;
            } else {
                if (tab_count == 0) {
                    tab_cmd_count = count;
                    tab_idx = 0;
                }
                tab_idx = (tab_idx + 1) % tab_cmd_count;

                int len_so_far = pos;
                if (buf[0] == '/') len_so_far--;
                int cmd_len = strlen(tab_matches[tab_idx]);

                for (int i = 0; i < len_so_far; i++) printf("\b \b");
                pos -= len_so_far;
                for (int i = 0; i < cmd_len; i++) {
                    buf[pos++] = tab_matches[tab_idx][i];
                    printf("%c", buf[pos-1]);
                }
                fflush(stdout);
                tab_count++;
            }
        } else if (key.wVirtualKeyCode == VK_UP) {
            if (hist_pos > 0) {
                hist_pos--;
                while (pos > 0) { pos--; printf("\b \b"); }
                int len = strlen(history[hist_pos]);
                strncpy(buf, history[hist_pos], MAX_LINE - 1);
                pos = len;
                printf("%s", buf);
                fflush(stdout);
            }
        } else if (key.wVirtualKeyCode == VK_DOWN) {
            if (hist_pos < hist_count) {
                hist_pos++;
                while (pos > 0) { pos--; printf("\b \b"); }
                if (hist_pos < hist_count) {
                    int len = strlen(history[hist_pos]);
                    strncpy(buf, history[hist_pos], MAX_LINE - 1);
                    pos = len;
                    printf("%s", buf);
                }
                fflush(stdout);
            }
        } else if (key.uChar.AsciiChar != 0) {
            if (pos < MAX_LINE - 1) {
                buf[pos++] = key.uChar.AsciiChar;
                printf("%c", key.uChar.AsciiChar);
                fflush(stdout);
            }
            tab_count = 0;
        }
    }
    SetConsoleMode(hIn, old_mode);
#else
    if (piped) {
        while (fgets(buf, MAX_LINE, stdin)) {
            size_t len = strlen(buf);
            while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
            if (len == 0) { printf("__EMT_READY__\n"); fflush(stdout); continue; }
            if (g_larpino_chat && buf[0] != '/') {
                printf("  larpino :");
                fflush(stdout);
                larpino_chat(g_larpino, buf, larpino_print_token, NULL);
                printf("\n");
            } else {
                add_history(buf);
                handle_command(buf);
            }
            fflush(stdout);
            printf("__EMT_READY__\n");
            fflush(stdout);
        }
        return;
    }
    enable_raw_mode();
    while (1) {
        int c = getchar();
        if (c == EOF || c == 4) { /* EOF or Ctrl+D */
            printf("\n");
            break;
        }
        if (c == '\n') {
            printf("\n");
            buf[pos] = '\0';
            if (pos > 0) {
                add_history(buf);
                handle_command(buf);
            }
            pos = 0;
            tab_count = 0;
            printf(" %s", retro(">> "));
            fflush(stdout);
        } else if (c == 127 || c == 8) {
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
            tab_count = 0;
        } else if (c == '\t') {
            buf[pos] = '\0';
            char full_input[256];
            strncpy(full_input, buf, 255);
            int count = find_completions(full_input, tab_matches);
            if (count == 0) {
                tab_count = 0;
            } else if (count == 1) {
                int cmd_len = strlen(tab_matches[0]);
                int in_len = pos;
                for (int i = in_len; i < cmd_len; i++) {
                    buf[pos++] = tab_matches[0][i];
                    printf("%c", buf[pos-1]);
                }
                buf[pos++] = ' ';
                printf(" ");
                fflush(stdout);
                tab_count = 0;
            } else {
                if (tab_count == 0) {
                    tab_cmd_count = count;
                    tab_idx = 0;
                }
                tab_idx = (tab_idx + 1) % tab_cmd_count;
                int len_so_far = pos;
                int cmd_len = strlen(tab_matches[tab_idx]);
                for (int i = 0; i < len_so_far; i++) printf("\b \b");
                pos -= len_so_far;
                for (int i = 0; i < cmd_len; i++) {
                    buf[pos++] = tab_matches[tab_idx][i];
                    printf("%c", buf[pos-1]);
                }
                fflush(stdout);
                tab_count++;
            }
        } else if (c == 27) {
            getchar();
            c = getchar();
            if (c == 'A' && hist_pos > 0) {
                hist_pos--;
                while (pos > 0) { pos--; printf("\b \b"); }
                int len = strlen(history[hist_pos]);
                strncpy(buf, history[hist_pos], MAX_LINE - 1);
                pos = len;
                printf("%s", buf);
                fflush(stdout);
            } else if (c == 'B' && hist_pos < hist_count) {
                hist_pos++;
                while (pos > 0) { pos--; printf("\b \b"); }
                if (hist_pos < hist_count) {
                    int len = strlen(history[hist_pos]);
                    strncpy(buf, history[hist_pos], MAX_LINE - 1);
                    pos = len;
                    printf("%s", buf);
                }
                fflush(stdout);
            }
        } else if (c > 31) {
            if (pos < MAX_LINE - 1) {
                buf[pos++] = c;
                printf("%c", c);
                fflush(stdout);
            }
            tab_count = 0;
        }
    }
    disable_raw_mode();
#endif
}
