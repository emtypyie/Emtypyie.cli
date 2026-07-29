#include "bakafetch.h"
#include "theme.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "bakafetch_data.h"

/* ─── Bakafetch (system info + ASCII art) module ───
 * Displays system information (OS, CPU, GPU, RAM, uptime) alongside
 * a random ASCII art and a tsundere-style message.
 *
 * All get_*() functions are cross-platform (#ifdef _WIN32).
 *
 * Future work:
 *  - Add disk usage, network stats, process count.
 *  - Add package manager detection.
 *  - User-customizable ASCII art packs (from ~/.emtypyie/arts/).
 *  - GPU detection on Linux via lspci or NVML.
 */

static int art_width(const char *line) {
    int w = 0;
    while (*line) {
        unsigned char c = (unsigned char)*line;
        if ((c & 0xF8) == 0xF0) { w += 2; line += 4; }
        else if ((c & 0xF0) == 0xE0) { w += 2; line += 3; }
        else if ((c & 0xE0) == 0xC0) { w += 1; line += 2; }
        else { w += 1; line += 1; }
    }
    return w;
}

static const char* get_os(void) {
    static char buf[128];
#ifdef _WIN32
    OSVERSIONINFOEX osvi;
    memset(&osvi, 0, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    GetVersionEx((OSVERSIONINFO*)&osvi);
    if (osvi.dwMajorVersion >= 10)
        snprintf(buf, sizeof(buf), "Windows 11 (%lu.%lu.%lu)", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
    else if (osvi.dwMajorVersion >= 6 && osvi.dwMinorVersion >= 3)
        snprintf(buf, sizeof(buf), "Windows 8.1 (%lu.%lu.%lu)", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
    else if (osvi.dwMajorVersion >= 6 && osvi.dwMinorVersion >= 2)
        snprintf(buf, sizeof(buf), "Windows 8 (%lu.%lu.%lu)", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
    else if (osvi.dwMajorVersion >= 6 && osvi.dwMinorVersion >= 1)
        snprintf(buf, sizeof(buf), "Windows 7 (%lu.%lu.%lu)", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
    else
        snprintf(buf, sizeof(buf), "Windows (%lu.%lu.%lu)", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
#else
    snprintf(buf, sizeof(buf), "Linux");
#endif
    return buf;
}

static const char* get_hostname(void) {
    static char buf[256];
#ifdef _WIN32
    DWORD size = sizeof(buf);
    GetComputerNameA(buf, &size);
#else
    gethostname(buf, sizeof(buf));
#endif
    return buf;
}

static const char* get_cpu(void) {
#ifdef _WIN32
    static char buf[256];
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(buf);
        RegQueryValueEx(hKey, "ProcessorNameString", NULL, NULL, (BYTE*)buf, &size);
        RegCloseKey(hKey);
        return buf;
    }
    return "Unknown CPU";
#else
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) return "Unknown CPU";
    static char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        if (strncmp(buf, "model name", 10) == 0) {
            char *p = strchr(buf, ':');
            if (p) { p += 2; char *nl = strchr(p, '\n'); if (nl) *nl = '\0'; fclose(f); return p; }
        }
    }
    fclose(f);
    return "Unknown CPU";
#endif
}

static int get_cpu_cores(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

static const char* get_gpu(void) {
    static char buf[512] = "Unknown";
#ifdef _WIN32
    char cmd[] = "powershell -Command \"Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name\"";
    char out[2048] = {0};
    if (exec_cmd(cmd, out, sizeof(out)) == 0) {
        char *nl = strchr(out, '\n'); if (nl) *nl = '\0';
        char *cr = strchr(out, '\r'); if (cr) *cr = '\0';
        if (strlen(out) > 0) { size_t len = strlen(out); if (len < sizeof(buf)) memcpy(buf, out, len + 1); }
    }
#endif
    return buf;
}

static long long get_total_ram(void) {
#ifdef _WIN32
    MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    return (long long)(ms.ullTotalPhys / (1024 * 1024));
#else
    struct sysinfo si; sysinfo(&si);
    return (long long)(si.totalram * si.mem_unit / (1024 * 1024));
#endif
}

static long long get_free_ram(void) {
#ifdef _WIN32
    MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    return (long long)(ms.ullAvailPhys / (1024 * 1024));
#else
    struct sysinfo si; sysinfo(&si);
    return (long long)(si.freeram * si.mem_unit / (1024 * 1024));
#endif
}

static const char* fmt_mem(long long mb) {
    static char buf[32];
    if (mb >= 1024) snprintf(buf, sizeof(buf), "%.1f GiB", mb / 1024.0);
    else snprintf(buf, sizeof(buf), "%lld MiB", mb);
    return buf;
}

static const char* get_uptime(void) {
    static char buf[32];
#ifdef _WIN32
    long long secs = GetTickCount64() / 1000;
#else
    struct sysinfo si; sysinfo(&si);
    long long secs = si.uptime;
#endif
    int d = (int)(secs / 86400); secs %= 86400;
    int h = (int)(secs / 3600); secs %= 3600;
    int m = (int)(secs / 60);
    char *p = buf;
    if (d > 0) p += sprintf(p, "%dd ", d);
    if (h > 0) p += sprintf(p, "%dh ", h);
    p += sprintf(p, "%dm", m);
    return buf;
}

static const char* get_build(void) {
#ifdef _WIN32
    return "windows";
#else
    return "linux";
#endif
}

void bakafetch_show(void) {
    srand((unsigned)time(NULL));
    int art_idx = rand() % ART_COUNT;
    int tsun_idx = rand() % TSUN_COUNT;
    const char **art = ARTS[art_idx];
    const char *tsun = TSUNDERE[tsun_idx];

    int max_art_w = 0, art_lines = 0;
    for (int i = 0; art[i]; i++) {
        int w = art_width(art[i]);
        if (w > max_art_w) max_art_w = w;
        art_lines++;
    }

    const char *hn = get_hostname();
    const char *os = get_os();
    const char *cpu = get_cpu();
    int cores = get_cpu_cores();
    const char *gpu = get_gpu();
    long long total_ram = get_total_ram();
    long long free_ram = get_free_ram();
    long long used_ram = total_ram - free_ram;

    char cpu_line[256];
    snprintf(cpu_line, sizeof(cpu_line), "%s (%d)", cpu, cores);
    char mem_line[64];
    snprintf(mem_line, sizeof(mem_line), "%s / %s", fmt_mem(used_ram), fmt_mem(total_ram));

    const char *labels[] = {"OS", "HOST", "KERNEL", "UPTIME", "SHELL", "CPU", "GPU", "MEMORY", "BUILD"};
    const char *vals[] = {os, hn, "emtypyie cli v3.0.2", get_uptime(), "emtypyie", cpu_line, gpu, mem_line, get_build()};
    int n = sizeof(labels) / sizeof(labels[0]);

    char header[128];
    snprintf(header, sizeof(header), "EMTYPYIE@%s", hn);
    int hl = (int)strlen(header);

    int right_rows = n + 2;
    int total_rows = art_lines > right_rows ? art_lines : right_rows;

    printf("\n");
    for (int i = 0; i < total_rows; i++) {
        if (i < art_lines) {
            int w = art_width(art[i]);
            int pad = max_art_w - w;
            printf("  %s", retro_accent(art[i]));
            if (pad) printf("%*s", pad, "");
            printf("   ");
        } else {
            printf("  %*s   ", max_art_w + 2, "");
        }

        if (i == 0) {
            printf("%s", retro_accent("EMTYPYIE"));
            printf("%s", retro_dim("@"));
            printf("%s", retro_accent(hn));
        } else if (i == 1) {
            char sep[128]; int seplen = 0;
            for (int j = 0; j < hl && seplen < 125; j++) {
                sep[seplen++] = '\xE2'; sep[seplen++] = '\x94'; sep[seplen++] = '\x80';
            }
            sep[seplen] = '\0';
            printf("%s", retro_dim(sep));
        } else {
            int idx = i - 2;
            if (idx < n) {
                int llen = (int)strlen(labels[idx]);
                int pad = 9 - llen;
                printf("%s", retro_accent(labels[idx]));
                printf("%s", retro_dim("\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"));
                if (pad > 0) printf("%*s", pad, "");
                printf("%s  ", retro_dim("\xC2\xB7"));
                printf("%s", retro_accent(vals[idx]));
            }
        }
        printf("\n");
    }

    printf("\n  %s\n", retro_dim(tsun));
    printf("\n");
}
