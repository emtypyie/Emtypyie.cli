#include "fetch.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#endif
#else
#include <curl/curl.h>
#endif

/* ─── HTTP fetch module ───
 * Makes HTTP GET requests. Uses WinHTTP on Windows, libcurl on POSIX.
 *
 * Future work:
 *  - Add POST/PUT support for API interactions.
 *  - Add progress callback for large downloads.
 *  - Add support for custom headers (e.g., Authorization).
 *
 * Usage:
 *   FetchResult *res = fetch_get_with_timeout("https://...", 30);
 *   if (res && res->status_code == 200) { ... use res->body ... }
 *   fetch_free(res);
 */

struct WriteBuf {
    char *data;
    size_t len;
    size_t cap;
};

static size_t write_cb(void *ptr, size_t size, size_t nmemb, struct WriteBuf *buf) {
    size_t total = size * nmemb;
    if (buf->len + total >= buf->cap) {
        buf->cap = buf->len + total + 4096;
        char *newdata = realloc(buf->data, buf->cap);
        if (!newdata) return 0;
        buf->data = newdata;
    }
    memcpy(buf->data + buf->len, ptr, total);
    buf->len += total;
    buf->data[buf->len] = '\0';
    return total;
}

#ifdef _WIN32
static void set_winhttp_error(FetchResult *res, const char *context) {
    DWORD err = GetLastError();
    char *msg = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*)&msg, 0, NULL);
    if (msg) {
        char *nl = strchr(msg, '\r'); if (nl) *nl = '\0';
        snprintf(res->error, sizeof(res->error), "%s: %s", context, msg);
        LocalFree(msg);
    } else {
        snprintf(res->error, sizeof(res->error), "%s: error %lu", context, err);
    }
}

static FetchResult* fetch_winhttp(const char *url, int timeout_sec) {
    FetchResult *res = calloc(1, sizeof(FetchResult));
    res->status_code = 0;
    res->error[0] = '\0';

    URL_COMPONENTS uc = { sizeof(uc) };
    wchar_t host[256], path[2048], scheme[16];
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    uc.lpszScheme = scheme; uc.dwSchemeLength = 16;

    wchar_t wurl[4096];
    mbstowcs(wurl, url, 4096);

    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) {
        set_winhttp_error(res, "bad URL");
        return res;
    }

    BOOL use_ssl = (wcscmp(scheme, L"https") == 0);
    DWORD port = uc.nPort ? uc.nPort : (use_ssl ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);

    HINTERNET hSession = WinHttpOpen(L"emtypyie-cli/3.0.2",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) { set_winhttp_error(res, "WinHttpOpen"); return res; }

    if (timeout_sec > 0) {
        int ms = timeout_sec * 1000;
        WinHttpSetTimeouts(hSession, ms, ms, -1, -1);
    }

    HINTERNET hConnect = WinHttpConnect(hSession, host, port, 0);
    if (!hConnect) { set_winhttp_error(res, "WinHttpConnect"); WinHttpCloseHandle(hSession); return res; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        use_ssl ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { set_winhttp_error(res, "WinHttpOpenRequest"); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return res; }

    if (timeout_sec > 0) {
        int ms = timeout_sec * 1000;
        WinHttpSetTimeouts(hRequest, -1, -1, ms, ms);
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        set_winhttp_error(res, "WinHttpSendRequest");
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return res;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        set_winhttp_error(res, "WinHttpReceiveResponse");
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return res;
    }

    DWORD status = 0, status_size = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        NULL, &status, &status_size, NULL);
    res->status_code = (int)status;

    struct WriteBuf buf = {0};
    buf.cap = 8192;
    buf.data = malloc(buf.cap);
    buf.data[0] = '\0';

    char tmp[8192];
    DWORD bytes_read;
    while (WinHttpReadData(hRequest, tmp, sizeof(tmp) - 1, &bytes_read) && bytes_read > 0) {
        tmp[bytes_read] = '\0';
        write_cb(tmp, 1, bytes_read, &buf);
    }

    res->body = buf.data;
    res->body_size = buf.len;

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return res;
}
#else
static FetchResult* fetch_curl(const char *url, int timeout_sec) {
    FetchResult *res = calloc(1, sizeof(FetchResult));
    res->status_code = 0;
    res->error[0] = '\0';

    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(res->error, sizeof(res->error), "curl_easy_init failed");
        return res;
    }

    struct WriteBuf buf = {0};
    buf.cap = 8192;
    buf.data = malloc(buf.cap);
    buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "emtypyie-cli/3.0.2");
    if (timeout_sec > 0) curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout_sec);

    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res->status_code);
        res->body = buf.data;
        res->body_size = buf.len;
    } else {
        snprintf(res->error, sizeof(res->error), "curl: %s", curl_easy_strerror(rc));
        free(buf.data);
    }

    curl_easy_cleanup(curl);
    return res;
}
#endif

FetchResult* fetch_get(const char *url) {
    return fetch_get_with_timeout(url, 30);
}

FetchResult* fetch_get_with_timeout(const char *url, int timeout_sec) {
    FetchResult *res = NULL;

    for (int attempt = 1; attempt <= 3; attempt++) {
        if (res) fetch_free(res);

#ifdef _WIN32
        res = fetch_winhttp(url, timeout_sec);
#else
        res = fetch_curl(url, timeout_sec);
#endif
        if (!res) continue;

        if (res->status_code >= 200 && res->status_code < 500) {
            break;
        }

        if (attempt < 3) {
#ifdef _WIN32
            Sleep(1000);
#else
            sleep(1);
#endif
        }
    }

    return res;
}

void fetch_free(FetchResult *res) {
    if (res) {
        free(res->body);
        free(res);
    }
}
