#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#include "theme.h"
#include "shell.h"
#include "util.h"

/* ─── Startup boot animation (Linux kernel style) ───
 * Embedded directly in main.c so it needs no external JSON file.
 * Skippable via --no-animation flag or EMTYPYIE_NO_ANIM env var.
 */

static const char *ANIM_LINES[] = {
    "[  OK  ] Initializing emtypyie-core v2.5.4 (Wandering Witches)",
    "[  OK  ] Architecture: x86_64-windows-nt",
    "[  OK  ] Memory arena allocated: 2.1 GiB / 16 GiB available",
    "[  OK  ] Page table initialized: 4KB pages, 52-bit virtual addressing",
    "[  OK  ] CPU features detected: AVX2, FMA, SSE4.2, POPCNT, BMI1/2",
    "[  OK  ] High-resolution timer: TSC @ 3.4 GHz calibrated",
    "[  OK  ] Interrupt controller: APIC x2APIC mode enabled",
    "[  OK  ] NUMA topology: 1 node, 8 cores, 16 threads detected",
    "[  OK  ] L1 cache: 32KB data + 32KB instruction per core",
    "[  OK  ] L2 cache: 256KB per core, L3 cache: 16MB shared",
    "[  OK  ] Memory bandwidth: ~45 GB/s dual-channel DDR4-3200",
    "[  OK  ] Boot loader: Windows Boot Manager 10.0.19045",
    "[  OK  ] Kernel command line: BOOT_IMAGE=\\Windows\\system32\\winload.efi",
    "[  OK  ] ACPI tables loaded: FACP, APIC, FPDT, SSDT, UEFI",
    "[  OK  ] PCIe topology: Root complex 0000:00:00.0, 3 endpoints",
    "[  OK  ] NVMe controller: Samsung 970 EVO Plus 1TB detected",
    "[  OK  ] Block layer: 512B logical, 4KB physical sectors",
    "[  OK  ] Filesystem: NTFS 3.1 mounted on C:\\ (1.8TB/2.0TB free)",
    "[  OK  ] Console: VT100 virtual terminal sequences enabled",
    "[  OK  ] Codepage: UTF-8 (65001) active for stdout/stderr",
    "[  OK  ] Loading module: core/emtypyie-core (static)",
    "[  OK  ] Symbol table loaded: 2,847 symbols resolved",
    "[  OK  ] Loading module: core/theme-engine (violet, slate, green, amber, cyan)",
    "[  OK  ] Theme registry: 5 themes, 128 color slots each",
    "[  OK  ] Loading module: core/bakafetch (braille renderer v3.1)",
    "[  OK  ] Braille font: 256 glyphs, 8-dot patterns loaded",
    "[  OK  ] System info providers: CPU, GPU, RAM, Disk, OS, Kernel",
    "[  OK  ] Loading module: core/fetch (HTTPS/CDN client)",
    "[  OK  ] HTTP backend: WinHTTP 10.0 (Windows) / libcurl 8.4 (Unix)",
    "[  OK  ] TLS context: TLS 1.3, cipher suites: AES-GCM, CHACHA20-POLY1305",
    "[  OK  ] CDN endpoints: cdn.emtypyie.in, api.emtypyie.in resolved",
    "[  OK  ] Loading module: core/download (multi-threaded)",
    "[  OK  ] Download engine: 4 concurrent streams, resume support",
    "[  OK  ] Checksum verification: SHA256, BLAKE3 hardware accelerated",
    "[  OK  ] Loading module: core/project (registry manager)",
    "[  OK  ] Project index: 47 projects, 12 categories, 3.2MB metadata",
    "[  OK  ] Dependency resolver: topological sort, cycle detection",
    "[  OK  ] Loading module: core/runtime (compiler detector)",
    "[  OK  ] GCC toolchain: gcc.exe 13.2.0 (mingw64) found in PATH",
    "[  OK  ] Clang toolchain: clang.exe 17.0.1 found in PATH",
    "[  OK  ] Node.js runtime: node.exe 20.12.0 found in PATH",
    "[  OK  ] Python runtime: python.exe 3.13.0 found in PATH",
    "[  OK  ] Loading module: core/larpino (LLM inference engine)",
    "[  OK  ] Larpino: GGUF parser v3 (Q4_0, Q4_1, Q5_0, Q5_1, Q8_0, F16, F32)",
    "[  OK  ] BPE tokenizer: 128,256 tokens, 350k merge rules loaded",
    "[  OK  ] Model loader: mmap() zero-copy, lazy page-in enabled",
    "[  OK  ] KV cache allocator: 32 layers x 4096 ctx x 32 heads x 128 dim",
    "[  OK  ] Attention: FlashAttention-2 kernel (AVX2/FMA path)",
    "[  OK  ] RoPE: Rotary positional embeddings precomputed to 8192",
    "[  OK  ] SwiGLU: Fused silu+gate+up projection, BF16 accumulation",
    "[  OK  ] RMSNorm: Vectorized eps=1e-6, 64-byte aligned",
    "[  OK  ] GQA: Grouped-query attention (8 groups, 32 heads)",
    "[  OK  ] Sampler: Top-k=40, Top-p=0.95, temp=0.8, min-p=0.05",
    "[  OK  ] Grammar constraints: GBNF parser for structured output",
    "[  OK  ] Loading module: core/shell (interactive REPL)",
    "[  OK  ] Readline: tab completion, history, vi/emacs modes",
    "[  OK  ] Command registry: 23 built-in commands registered",
    "[  OK  ] Alias system: 12 user aliases loaded from config",
    "[  OK  ] Loading module: core/auth (credential manager)",
    "[  OK  ] Secret store: Windows Credential Manager / libsecret",
    "[  OK  ] GitHub token: ******** (scopes: repo, workflow, gist)",
    "[  OK  ] NPM token: ******** (registry: npm.emtypyie.in)",
    "[  OK  ] Chocolatey key: ******** (source: choco.emtypyie.in)",
    "[  OK  ] Loading module: core/runtime-install (compiler bootstrap)",
    "[  OK  ] MinGW-w64: 13.2.0 package cached (247MB)",
    "[  OK  ] LLVM/Clang: 17.0.1 package cached (1.2GB)",
    "[  OK  ] Bootstrapping: ccache 4.9.1, ninja 1.12.1 ready",
    "[  OK  ] Loading module: core/publish (release automation)",
    "[  OK  ] Git: 2.45.0, GitHub CLI: 2.48.0, npm: 10.5.0",
    "[  OK  ] Signing: cosign 2.2.4, GPG: 2.4.5 (ED25519 key)",
    "[  OK  ] Loading module: core/wrap (project packaging)",
    "[  OK  ] Git staging: diff, status, add, commit, push pipeline",
    "[  OK  ] npm publish: version bump, changelog, provenance",
    "[  OK  ] GitHub Repo API: create, PR, release, deploy keys",
    "[  OK  ] Configuration: ~/.emtypyie/config.json parsed",
    "[  OK  ] Theme preference: violet (saved 2026-07-17)",
    "[  OK  ] Bakafetch color: violet (matches theme)",
    "[  OK  ] Shell prompt: retro-accent '>> ' with git branch",
    "[  OK  ] History file: ~/.emtypyie/history (1000 entries)",
    "[  OK  ] Log level: INFO (default), DEBUG available via --verbose",
    "[  OK  ] Telemetry: disabled (opt-in only)",
    "[  OK  ] Update channel: stable (github.com/emtypyie/Emtypyie.cli)",
    "[  OK  ] Version check: current 2.5.4, latest 2.5.4 (up to date)",
    "[  OK  ] Plugin system: 0 external plugins loaded",
    "[  OK  ] Sandbox: project isolation via separate working dirs",
    "[  OK  ] Resource limits: max 4GB RAM, 50% CPU per project",
    "[  OK  ] Signal handlers: SIGINT, SIGTERM, SIGBREAK registered",
    "[  OK  ] Crash reporter: minidump to ~/.emtypyie/crashes/",
    "[  OK  ] Loading module: net/cdn-client",
    "[  OK  ] CDN health: 3/3 edge nodes responsive (<50ms)",
    "[  OK  ] Manifest: dev branch, 47 projects, 12.4MB compressed",
    "[  OK  ] ETag cache: 34 entries valid, 13 stale (will refresh)",
    "[  OK  ] Compression: Brotli q=11, Zstd q=19, Gzip q=9",
    "[  OK  ] Loading module: net/github-api",
    "[  OK  ] GitHub API: rate limit 5000/hr, 4987 remaining",
    "[  OK  ] GraphQL endpoint: api.github.com/graphql ready",
    "[  OK  ] REST endpoint: api.github.com REST v3 ready",
    "[  OK  ] Loading module: ui/progress (download bars)",
    "[  OK  ] Progress renderer: Unicode blocks, 60fps cap",
    "[  OK  ] Spinner: 12-frame braille, 80ms/frame",
    "[  OK  ] Loading module: ui/banner (ASCII art)",
    "[  OK  ] Banner: 12 lines, 78 cols, braille witch art",
    "[  OK  ] Loading module: util/fs (filesystem abstraction)",
    "[  OK  ] Path resolver: Windows \\\\?\\ prefix, UNC support",
    "[  OK  ] Atomic write: temp + rename, fsync on close",
    "[  OK  ] Directory watcher: ReadDirectoryChangesW / inotify",
    "[  OK  ] Loading module: util/process (spawn wrapper)",
    "[  OK  ] Job objects: Windows child process groups",
    "[  OK  ] PTY emulation: ConPTY API (Windows 10 1809+)",
    "[  OK  ] Loading module: util/encoding (charset)",
    "[  OK  ] UTF-8: native on Windows 10+, iconv fallback",
    "[  OK  ] Loading module: util/time (monotonic clock)",
    "[  OK  ] QueryPerformanceCounter: 100ns resolution",
    "[  OK  ] Loading module: util/hash (checksums)",
    "[  OK  ] SHA256: Intel SHA-NI, fallback: OpenSSL 3.2",
    "[  OK  ] BLAKE3: SIMD (AVX2/SSE4.1), 3.2 GB/s single-thread",
    "[  OK  ] Loading module: util/json (cJSON 1.7.18)",
    "[  OK  ] Parser: streaming, 1.2M tokens/sec",
    "[  OK  ] Loading module: util/args (CLI parser)",
    "[  OK  ] Flag parser: short/long, positional, subcommands",
    "[  OK  ] Completion: bash, zsh, fish, PowerShell generators",
    "[  OK  ] Loading module: core/doctor (self-diagnostics)",
    "[  OK  ] Health checks: 12/12 passing (network, disk, perms)",
    "[  OK  ] Doctor: PATH, compiler, runtime, tokens, config OK",
    "[  OK  ] Loading module: core/migrate (config upgrades)",
    "[  OK  ] Migration v2.5.x -> v2.5.4: 0 steps required",
    "[  OK  ] Finalizing: flushing stdout/stderr buffers",
    "[  OK  ] Terminal reset: cursor position (0,0), alternate screen",
    "[  OK  ] Signal mask: SIGWINCH handler for resize events",
    "[  OK  ] Emtypyie CLI v2.5.4 ready -- type /help for commands",
    "[  OK  ] Welcome back. The witches are wandering.",
    "[  OK  ] ████████████████████████████████████████ 100%"
};

#define ANIM_LINE_COUNT (sizeof(ANIM_LINES) / sizeof(ANIM_LINES[0]))
#define ANIM_TOTAL_MS 2000
#define ANIM_TARGET_COUNT 300

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

    int interval_ms = ANIM_TOTAL_MS / ANIM_TARGET_COUNT;

    for (int i = 0; i < ANIM_TARGET_COUNT; i++) {
        const char *line = ANIM_LINES[i % ANIM_LINE_COUNT];
        print_line_padded(line, target_width);
        fflush(stdout);
        if (interval_ms > 0) Sleep(interval_ms);
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
    char *emty_dir = get_emty_dir();

    char theme_path[1024];
    snprintf(theme_path, sizeof(theme_path), "%s%ctheme.txt", emty_dir, PATH_SEP);
    if (file_exists(theme_path)) {
        char *saved = read_file(theme_path);
        if (saved) {
            char *nl = strchr(saved, '\n');
            if (nl) *nl = '\0';
            theme_name = saved;
        }
    }

    theme_init(theme_name);

    int skip_anim = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-animation") == 0) skip_anim = 1;
    }

    if (argc == 1) {
        if (!skip_anim && !getenv("EMTYPYIE_NO_ANIM")) {
            startup_animation_play();
        }
        shell_run();
        return 0;
    }

    const char *cmd = argv[1];
    const char *arg = argc > 2 ? argv[2] : "";

    if (cmd[0] != '/') {
        if (strcmp(cmd, "help") == 0) print_usage();
        else if (strcmp(cmd, "about") == 0)
            printf("EMTYPYIE CLI v2.5.4 (C port)\n");
        else
            printf("Unrecognized. Use /help\n");
        return 0;
    }

    if      (strcmp(cmd, "/help") == 0)  print_usage();
    else if (strcmp(cmd, "/about") == 0) {
        printf("  %s\n", retro_accent("EMTYPYIE CLI v2.5.4"));
        printf("  %s\n", retro_dim("\"Wandering Witches\""));
        printf("\n");
        printf("  %s\n", retro_accent(
            "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa2\xa2\xe2\xa1\x84\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80"
        ));
        printf("  %s\n", retro_accent(
            "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa3\xa9\xe2\xa3\xa4\xe2\xa1\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x8d\xe2\xa0\xad\xe2\xa1\x94\xe2\xa1\x84\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x81\xe2\xa3\xbf\xe2\xa3\x86\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80"
        ));
        printf("  %s\n", retro_accent(
            "\xe2\xb0\xa2\xe2\x82\x82\xe2\xa3\xa9\xe2\xa3\xa4\xe2\xa3\xa4\xe2\xa3\xa4\xe2\xa3\xa9\xe2\xa0\xb9\xe2\xa3\xb7\xe2\xa3\xa4\xe2\xa3\xa9\xe2\xa3\xa9\xe2\xa0\x80\xe2\xa0\x89\xe2\xa3\xbf\xe2\xa3\xb6\xe2\xa3\xb6\xe2\xa3\xb6\xe2\xa3\xb6\xe2\xa3\xb6\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\xb7\xe2\xa0\x86\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa3\xa9\xe2\xa3\xa4\xe2\xa0\x80\xe2\xa0\x80"
        ));
        printf("  %s\n", retro_accent(
            "\xe2\xa0\x80\xe2\xa0\x88\xe2\xa0\x89\xe2\xa0\xb9\xe2\xa0\x9f\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x8b\xe2\xa0\x80\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\x9b\xe2\xa0\x8b\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\xa4\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa3\xbf\xe2\xa3\x8f\xe2\xa0\x80\xe2\xa0\x80"
        ));
        printf("  %s\n", retro_accent(
            "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x90\xe2\xa0\xb9\xe2\xa0\x9f\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\xbf\xe2\xa0\x8b\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\xb8\xe2\xa0\x81\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\x80\xe2\xa0\x80"
        ));
        printf("  %s\n", retro_accent(
            "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\xa0\xe2\xa0\xb4\xe2\xa3\xbf\xe2\xa0\x9f\xe2\xa0\x9b\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\x9f\xe2\xa3\xa9\xe2\xa3\xa4\xe2\xa3\xb6\xe2\xa3\xbe\xe2\xa3\xbf\xe2\xa0\x89\xe2\xa0\xb9\xe2\xa0\xa6\xe2\xa0\x88\xe2\xa0\xb3\xe2\xa0\xa4\xe2\xa1\x80\xe2\xa0\x80\xe2\xa0\x88\xe2\xa0\xbc\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa1\x84\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80"
        ));
        printf("  %s\n", retro_accent(
            "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x8b\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x8b\xe2\xa0\x80\xe2\xa0\x80\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\x8f\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80"
        ));
        printf("  %s\n", retro_accent(
            "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\xba\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\xbf\xe2\xa0\xbf\xe2\xa0\xbf\xe2\xa3\x9f\xe2\xa3\x9b\xe2\xa3\xad\xe2\xa3\xbd\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\x8f\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80"
        ));
        printf("  %s\n", retro_accent(
            "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x89\xe2\xa0\x95\xe2\xa0\xba\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\xbf\xe2\xa0\x83\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80"
        ));
        printf("  %s\n", retro_accent(
            "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\xa0\xe2\xa0\xb4\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\xbf\xe2\xa0\x83\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80"
        ));
        printf("  %s\n", retro_accent(
            "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\xa0\xe2\xa0\xb4\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\xbf\xe2\xa0\x83\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80"
        ));
        printf("  %s\n", retro_accent(
            "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x88\xe2\xa0\x89\xe2\xa0\x80\xe2\xa0\x81\xe2\xa0\x9b\xe2\xa0\xbb\xe2\xa3\xbf\xe2\xa0\x9f\xe2\xa0\x91\xe2\xa0\xbe\xe2\xa0\x81\xe2\xa3\xbf\xe2\xa0\x81\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80"
        ));
        printf("  %s\n", retro_accent(
            "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x98\xe2\xa0\x81\xe2\xa0\x80\xe2\xa0\x81\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80"
        ));
        printf("\n");
        printf("  %s  %s\n", retro_dim("Release:"), retro_accent("Wandering Witches"));
        printf("  %s\n", retro_dim("Copyright 2026 EMTYPYIE. All rights reserved."));
    }
    else if (strcmp(cmd, "/get") == 0) {
        if (strlen(arg) == 0) { printf("  Specify a project.\n"); return 1; }
        if (strcmp(arg, "gcc") == 0 || strcmp(arg, "g++") == 0) {
            extern void runtime_install_compiler(const char*);
            runtime_install_compiler(arg);
        } else {
            extern void project_get(const char*);
            project_get(arg);
        }
    }
    else if (strcmp(cmd, "/info") == 0) {
        if (strlen(arg) == 0) { printf("  Specify a project.\n"); return 1; }
        extern void project_info(const char*);
        project_info(arg);
    }
    else if (strcmp(cmd, "/list") == 0) {
        extern void project_list(void);
        project_list();
    }
    else if (strcmp(cmd, "/flash") == 0) {
        if (strlen(arg) == 0) { printf("  Specify a project.\n"); return 1; }
        extern void project_flash(const char*);
        project_flash(arg);
    }
    else if (strcmp(cmd, "/rm") == 0) {
        if (strlen(arg) == 0) { printf("  Specify a project.\n"); return 1; }
        extern void project_remove(const char*);
        project_remove(arg);
    }
    else if (strcmp(cmd, "/theme") == 0) {
        if (strlen(arg) == 0) {
            printf("  Current theme: %s\n", theme_current());
            printf("  Available: %s\n", theme_list());
        } else {
            theme_init(arg);
            printf("  Theme set to %s\n", retro(theme_current()));
            char theme_path[1024];
            snprintf(theme_path, sizeof(theme_path), "%s%ctheme.txt", get_emty_dir(), PATH_SEP);
            write_file(theme_path, arg);
        }
    }
    else if (strcmp(cmd, "/docs") == 0) {
        if (strlen(arg) == 0) { printf("  Specify a project.\n"); return 1; }
        extern void project_docs(const char*);
        project_docs(arg);
    }
    else if (strcmp(cmd, "/bf") == 0 || strcmp(cmd, "/bakafetch") == 0) {
        extern void bakafetch_show(void);
        bakafetch_show();
    }
    else if (strcmp(cmd, "/larpino") == 0) {
        printf("  %s\n", retro_dim("Use /larpino from the interactive shell."));
    }
    else if (strcmp(cmd, "/clear") == 0) {
        printf("\033[2J\033[H");
    }
    else if (strcmp(cmd, "/shell") == 0) {
        shell_run();
    }
    else if (strcmp(cmd, "/setenv") == 0) {
        extern void auth_setenv(void);
        auth_setenv();
    }
    else {
        printf("  Unknown command. Use /help\n");
    }

    return 0;
}
