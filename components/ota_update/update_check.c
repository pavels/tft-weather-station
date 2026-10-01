#include <stdlib.h>
#include <string.h>

#include "update_check.h"

#include "HttpClient.h"
#include "ota_update.h"

#include "version.h"

/* Cloudflare Worker (tools/ota-version-worker): answers GET /latest with the
 * latest release tag, e.g. "1.2.0\n". Compiled into every unit: it must never
 * change once units are shipped. */
#define VERSION_HOST "tft-weather-station-version.pavels1.workers.dev"
#define VERSION_PATH "/latest"

/* After this many failed checks in a row (3 days of hourly checks: the version
 * service unreachable), restart into update mode anyway -- during the
 * power-off hours -- which checks GitHub directly. */
#define FALLBACK_AFTER_FAILURES 72

typedef struct {
    char* version;
    size_t size;
    size_t len;
} VersionResponse;

static void on_body(const char* data, int length, void* ctx) {
    VersionResponse* response = ctx;
    for (int i = 0; i < length && response->len + 1 < response->size; i++) {
        char c = data[i];
        if ((c >= '0' && c <= '9') || c == '.') response->version[response->len++] = c;
    }
    response->version[response->len] = '\0';
}

bool fetch_latest_version(char* version, size_t size) {
    VersionResponse response = { .version = version, .size = size, .len = 0 };
    version[0] = '\0';
    if (http_get(VERSION_HOST, VERSION_PATH, on_body, &response) != 1) return false;
    return response.len > 0;
}

int version_compare(const char* a, const char* b) {
    while (*a || *b) {
        char* a_end;
        char* b_end;
        long a_part = strtol(a, &a_end, 10);
        long b_part = strtol(b, &b_end, 10);
        if (a_part != b_part) return a_part < b_part ? -1 : 1;
        a = *a_end == '.' ? a_end + 1 : a_end;
        b = *b_end == '.' ? b_end + 1 : b_end;
        if (a == a_end && b == b_end) break;
    }
    return 0;
}

void ota_check_for_update(bool power_off_hours)
{
    static int failures;
    char latest[16];

    if (!fetch_latest_version(latest, sizeof(latest))) {
        if (++failures >= FALLBACK_AFTER_FAILURES && power_off_hours) {
            failures = 0;
            ota_request_update(NULL);
        }
        return;
    }
    failures = 0;
    if (version_compare(latest, OTAVERSION) > 0) ota_request_update(latest);
}
