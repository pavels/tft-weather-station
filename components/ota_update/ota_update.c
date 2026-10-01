#include "ota_update.h"

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "mbedtls/base64.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include "nvs.h"

#include "Controller.h"
#include "JsonFeed.h"
#include "update_check.h"
#include "update_screen.h"

#include "version.h"

static const char *TAG = "ota";

/* Update state in NVS: the flag that makes the next boot enter update mode,
 * the version the check announced ("" for a manual check), and the failed
 * attempts for the last announced version that failed. */
#define OTA_NVS_NAMESPACE "ota"
#define KEY_PENDING       "pending"
#define KEY_TARGET        "target"
#define KEY_FAIL_VERSION  "fail_ver"
#define KEY_FAIL_COUNT    "fail_count"
/* Trial of a freshly installed image: its version and unconfirmed boots. */
#define KEY_TRIAL_VERSION "trial_ver"
#define KEY_TRIAL_BOOTS   "trial_boots"
/* Unconfirmed boots of a new image before switching back to the previous one. */
#define MAX_TRIAL_BOOTS 3
/* An announced version is given up on after this many failed attempts, until
 * a different version is announced. Manual checks always run. */
#define MAX_ATTEMPTS 3

/* GitHub Releases. The manifest comes from the newest release (latest/download
 * redirects to it); the image from the release the verified manifest names. */
#define MANIFEST_URL "https://github.com/pavels/tft-weather-station/releases/latest/download/firmware.manifest"
#define FIRMWARE_URL_FORMAT "https://github.com/pavels/tft-weather-station/releases/download/v%s/firmware.bin"
/* The manifest's "name", so an image for another product is refused. */
#define PRODUCT_NAME "tft_weather_station"

/* Manifest: line 1 JSON (~150 bytes), line 2 the base64 signature (344 chars).
 * See tools/sign_release.py. */
#define MANIFEST_MAX   1024
#define SIGNATURE_LEN  256 /* RSA-2048 */
#define SHA256_LEN     32

/* esp_http_client receive (response headers) and transmit (request line)
 * buffers, bytes: GitHub's redirect target URL is ~1.3KB. */
#define HTTP_BUFFER_SIZE 2048
#define HTTP_TIMEOUT_MS  15000
#define MAX_REDIRECTS    5
/* Bytes per read from the connection and per flash write. */
#define READ_CHUNK       1024

/* How long update mode waits for WiFi before giving up, ms. */
#define WIFI_WAIT_MS     60000
/* How long the final status stays on screen before the restart, ms. */
#define RESTART_DELAY_MS 5000
/* ~3.0KB used, measured through TLS and RSA verification. */
#define UPDATE_TASK_STACK 5120
#define UPDATE_TASK_PRIO  4

/* ota_public_key.pem, embedded by this component's CMakeLists.txt
 * (NUL-terminated). */
extern const char ota_public_key_start[] asm("_binary_ota_public_key_pem_start");
extern const char ota_public_key_end[] asm("_binary_ota_public_key_pem_end");

typedef struct {
    char name[32];
    char version[16];
    long size;
    char sha256[2 * SHA256_LEN + 1]; /* lowercase hex */
} Manifest;

/* The version this update was started for ("" for a manual check); read by
 * ota_update_requested(). */
static char target_version[16];

/* This boot runs an image on trial (set by ota_trial_boot_check()). */
static bool on_trial;

static bool nvs_open_ota(nvs_open_mode mode, nvs_handle *handle)
{
    return nvs_open(OTA_NVS_NAMESPACE, mode, handle) == ESP_OK;
}

static void read_string(nvs_handle handle, const char *key, char *value, size_t size)
{
    value[0] = '\0';
    nvs_get_str(handle, key, value, &size);
}

bool ota_update_requested(void)
{
    nvs_handle handle;
    uint8_t pending = 0;
    if (!nvs_open_ota(NVS_READONLY, &handle)) return false;
    nvs_get_u8(handle, KEY_PENDING, &pending);
    read_string(handle, KEY_TARGET, target_version, sizeof(target_version));
    nvs_close(handle);
    return pending != 0;
}

void ota_request_update(const char *version)
{
    nvs_handle handle;
    if (!nvs_open_ota(NVS_READWRITE, &handle)) return;

    if (version != NULL) {
        char failed_version[16];
        uint8_t failures = 0;
        read_string(handle, KEY_FAIL_VERSION, failed_version, sizeof(failed_version));
        nvs_get_u8(handle, KEY_FAIL_COUNT, &failures);
        if (strcmp(failed_version, version) == 0 && failures >= MAX_ATTEMPTS) {
            ESP_LOGW(TAG, "not updating to %s: failed %u times", version, failures);
            nvs_close(handle);
            return;
        }
    }

    ESP_LOGI(TAG, "restarting into update mode (%s)", version ? version : "manual check");
    nvs_set_u8(handle, KEY_PENDING, 1);
    nvs_set_str(handle, KEY_TARGET, version ? version : "");
    nvs_commit(handle);
    nvs_close(handle);
    esp_restart();
}

/* Counts a failed attempt against `version` (commit left to the caller). */
static void record_failure(nvs_handle handle, const char *version)
{
    char failed_version[16];
    uint8_t failures = 0;
    read_string(handle, KEY_FAIL_VERSION, failed_version, sizeof(failed_version));
    nvs_get_u8(handle, KEY_FAIL_COUNT, &failures);
    if (strcmp(failed_version, version) != 0) failures = 0;
    nvs_set_str(handle, KEY_FAIL_VERSION, version);
    nvs_set_u8(handle, KEY_FAIL_COUNT, failures + 1);
}

/* Leaves update mode on the next boot; `failed` counts an attempt against the
 * announced version, `installed` clears the failure record. */
static void end_update(bool failed, bool installed)
{
    nvs_handle handle;
    if (!nvs_open_ota(NVS_READWRITE, &handle)) return;

    nvs_set_u8(handle, KEY_PENDING, 0);
    if (installed) {
        nvs_erase_key(handle, KEY_FAIL_VERSION);
        nvs_erase_key(handle, KEY_FAIL_COUNT);
    } else if (failed && target_version[0] != '\0') {
        record_failure(handle, target_version);
    }
    nvs_commit(handle);
    nvs_close(handle);
}

void ota_trial_boot_check(void)
{
    nvs_handle handle;
    char version[16];
    uint8_t boots = 0;
    if (!nvs_open_ota(NVS_READWRITE, &handle)) return;
    read_string(handle, KEY_TRIAL_VERSION, version, sizeof(version));
    if (version[0] == '\0') {
        nvs_close(handle);
        return;
    }

    nvs_get_u8(handle, KEY_TRIAL_BOOTS, &boots);
    if (strcmp(version, OTAVERSION) != 0) {
        /* Not the image on trial: the bootloader couldn't start it and fell
         * back. Recorded as a failed attempt so it isn't reinstalled forever. */
        ESP_LOGE(TAG, "%s was installed but %s is running", version, OTAVERSION);
        record_failure(handle, version);
        nvs_erase_key(handle, KEY_TRIAL_VERSION);
        nvs_erase_key(handle, KEY_TRIAL_BOOTS);
        nvs_commit(handle);
        nvs_close(handle);
        return;
    }
    if (boots < MAX_TRIAL_BOOTS) {
        ESP_LOGW(TAG, "%s on trial, unconfirmed boot %u of %u", version, boots + 1, MAX_TRIAL_BOOTS);
        nvs_set_u8(handle, KEY_TRIAL_BOOTS, boots + 1);
        nvs_commit(handle);
        nvs_close(handle);
        on_trial = true;
        return;
    }

    /* Ends the trial either way; the version counts as a failed attempt. */
    record_failure(handle, version);
    nvs_erase_key(handle, KEY_TRIAL_VERSION);
    nvs_erase_key(handle, KEY_TRIAL_BOOTS);
    nvs_commit(handle);
    nvs_close(handle);

    /* esp_ota_set_boot_partition() checks the previous image before switching. */
    const esp_partition_t *previous = esp_ota_get_next_update_partition(NULL);
    if (previous == NULL || esp_ota_set_boot_partition(previous) != ESP_OK) {
        ESP_LOGE(TAG, "%s failed its trial, but there is no valid previous image to go back to", version);
        return;
    }
    ESP_LOGE(TAG, "%s failed its trial (%u unconfirmed boots), going back to %s", version, boots, previous->label);
    esp_restart();
}

static void clear_trial(void)
{
    nvs_handle handle;
    if (!nvs_open_ota(NVS_READWRITE, &handle)) return;
    nvs_erase_key(handle, KEY_TRIAL_VERSION);
    nvs_erase_key(handle, KEY_TRIAL_BOOTS);
    nvs_commit(handle);
    nvs_close(handle);
}

void ota_confirm_boot(void)
{
    if (!on_trial) return;
    clear_trial();
    on_trial = false;
    ESP_LOGI(TAG, "%s confirmed, trial over", OTAVERSION);
}

/* Starts the trial of the image about to be booted. */
static bool start_trial(const char *version)
{
    nvs_handle handle;
    if (!nvs_open_ota(NVS_READWRITE, &handle)) return false;
    nvs_set_str(handle, KEY_TRIAL_VERSION, version);
    nvs_set_u8(handle, KEY_TRIAL_BOOTS, 0);
    bool ok = nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    return ok;
}

static void log_heap(const char *stage)
{
    ESP_LOGI(TAG, "%s: heap free %u (8bit %u), min ever %u (8bit %u), task stack free %u", stage,
             heap_caps_get_free_size(MALLOC_CAP_32BIT), heap_caps_get_free_size(MALLOC_CAP_8BIT),
             heap_caps_get_minimum_free_size(MALLOC_CAP_32BIT), heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT),
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
}

static void set_status(const char *status)
{
    update_screen_set_status(status);
    ESP_LOGI(TAG, "%s", status);
}

static void __attribute__((noreturn)) finish(const char *status, bool failed, bool installed)
{
    set_status(status);
    log_heap("done");
    end_update(failed, installed);
    vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
    esp_restart();
}

static bool wait_for_wifi(void)
{
    for (int waited = 0; !wifi_connected; waited += 100) {
        if (waited >= WIFI_WAIT_MS) return false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return true;
}

/* Opens the URL over HTTPS -- no certificate validation: trust comes from the
 * signed manifest -- following redirects. Returns the client positioned at
 * the body of the final 200 response, or NULL. */
static esp_http_client_handle_t open_url(const char *url, int *content_length)
{
    esp_http_client_config_t config = {
        .url = url,
        .buffer_size = HTTP_BUFFER_SIZE,
        .buffer_size_tx = HTTP_BUFFER_SIZE,
        .timeout_ms = HTTP_TIMEOUT_MS,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return NULL;

    for (int redirects = 0; redirects <= MAX_REDIRECTS; redirects++) {
        if (esp_http_client_open(client, 0) != ESP_OK) break;
        *content_length = esp_http_client_fetch_headers(client);
        log_heap("response headers");

        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP %d, content length %d", status, *content_length);
        if (status == 200) return client;
        if (status < 300 || status >= 400 || esp_http_client_set_redirection(client) != ESP_OK) break;
        esp_http_client_close(client);
    }

    esp_http_client_cleanup(client);
    return NULL;
}

/* Downloads the manifest into `buffer` as a NUL-terminated string. */
static bool download_manifest(char *buffer, size_t size)
{
    int content_length = 0;
    esp_http_client_handle_t client = open_url(MANIFEST_URL, &content_length);
    if (client == NULL) return false;

    size_t len = 0;
    int read;
    while (len + 1 < size && (read = esp_http_client_read(client, buffer + len, size - 1 - len)) > 0) {
        len += read;
    }
    buffer[len] = '\0';
    esp_http_client_cleanup(client);
    return len > 0;
}

static void on_manifest_value(const JsonValue *v, void *ctx)
{
    Manifest *manifest = ctx;
    if (strcmp(v->section, "") != 0) return;

    if (strcmp(v->key, "name") == 0) {
        strlcpy(manifest->name, v->text, sizeof(manifest->name));
    } else if (strcmp(v->key, "version") == 0) {
        strlcpy(manifest->version, v->text, sizeof(manifest->version));
    } else if (strcmp(v->key, "size") == 0) {
        manifest->size = strtol(v->text, NULL, 10);
    } else if (strcmp(v->key, "sha256") == 0) {
        strlcpy(manifest->sha256, v->text, sizeof(manifest->sha256));
    }
}

static bool verify_signature(const char *line, size_t line_len, const char *signature_b64)
{
    unsigned char signature[SIGNATURE_LEN];
    unsigned char hash[SHA256_LEN];
    size_t signature_len = 0;

    if (mbedtls_base64_decode(signature, sizeof(signature), &signature_len,
                              (const unsigned char *)signature_b64, strlen(signature_b64)) != 0 ||
        signature_len != SIGNATURE_LEN) {
        return false;
    }
    mbedtls_sha256_ret((const unsigned char *)line, line_len, hash, 0);

    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    int ret = mbedtls_pk_parse_public_key(&key, (const unsigned char *)ota_public_key_start,
                                          ota_public_key_end - ota_public_key_start);
    if (ret == 0) ret = mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, hash, sizeof(hash), signature, signature_len);
    mbedtls_pk_free(&key);
    return ret == 0;
}

/* Checks the signature of line 1 against line 2, then parses line 1. The
 * buffer is modified (lines split). */
static bool parse_manifest(char *buffer, Manifest *manifest)
{
    char *line_end = strchr(buffer, '\n');
    if (line_end == NULL) return false;
    *line_end = '\0';
    char *signature = line_end + 1;
    signature[strcspn(signature, "\r\n")] = '\0';

    if (!verify_signature(buffer, line_end - buffer, signature)) {
        ESP_LOGE(TAG, "manifest signature invalid");
        return false;
    }
    log_heap("signature verified");

    static JsonFeed feed;
    memset(manifest, 0, sizeof(*manifest));
    json_feed_init(&feed, on_manifest_value, manifest);
    json_feed_data(&feed, buffer, line_end - buffer);
    ESP_LOGI(TAG, "manifest: %s %s, %ld bytes, sha256 %s", manifest->name, manifest->version, manifest->size,
             manifest->sha256);
    return strcmp(manifest->name, PRODUCT_NAME) == 0 && manifest->version[0] != '\0' && manifest->size > 0 &&
           strlen(manifest->sha256) == 2 * SHA256_LEN;
}

static void to_hex(const unsigned char *data, size_t len, char *hex)
{
    static const char DIGITS[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        hex[2 * i] = DIGITS[data[i] >> 4];
        hex[2 * i + 1] = DIGITS[data[i] & 0xF];
    }
    hex[2 * len] = '\0';
}

/* Downloads the image into `slot` through esp_ota_*, checking size and SHA-256
 * against the manifest; true once the slot holds a complete, valid image. */
static bool install_firmware(const esp_partition_t *slot, const Manifest *manifest)
{
    esp_ota_handle_t ota;
    set_status("Erasing...");
    if (esp_ota_begin(slot, manifest->size, &ota) != ESP_OK) return false;

    set_status("Downloading...");
    char url[sizeof(FIRMWARE_URL_FORMAT) + sizeof(manifest->version)];
    snprintf(url, sizeof(url), FIRMWARE_URL_FORMAT, manifest->version);
    int content_length = 0;
    esp_http_client_handle_t client = open_url(url, &content_length);
    if (client == NULL) return false;
    if (content_length > 0 && content_length != manifest->size) {
        ESP_LOGE(TAG, "image is %d bytes, manifest says %ld", content_length, manifest->size);
        esp_http_client_cleanup(client);
        return false;
    }

    char *buffer = malloc(READ_CHUNK);
    static mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts_ret(&sha, 0);

    long written = 0;
    int percent = 0;
    bool ok = buffer != NULL;
    while (ok && written < manifest->size) {
        int len = esp_http_client_read(client, buffer, READ_CHUNK);
        if (len <= 0) break;
        if (written + len > manifest->size) {
            ok = false;
            break;
        }
        mbedtls_sha256_update_ret(&sha, (unsigned char *)buffer, len);
        ok = esp_ota_write(ota, buffer, len) == ESP_OK;
        written += len;
        if (written * 100 / manifest->size != percent) {
            percent = written * 100 / manifest->size;
            update_screen_set_progress(percent);
        }
        if (written % (64 * 1024) < len) log_heap("downloading");
    }
    esp_http_client_cleanup(client);
    free(buffer);

    unsigned char hash[SHA256_LEN];
    char hash_hex[2 * SHA256_LEN + 1];
    mbedtls_sha256_finish_ret(&sha, hash);
    mbedtls_sha256_free(&sha);
    to_hex(hash, sizeof(hash), hash_hex);
    ESP_LOGI(TAG, "downloaded %ld of %ld bytes, sha256 %s", written, manifest->size, hash_hex);

    set_status("Verifying...");
    if (!ok || written != manifest->size || strcmp(hash_hex, manifest->sha256) != 0) {
        ESP_LOGE(TAG, "download incomplete or hash mismatch");
        return false;
    }
    /* Checks the image format and checksum. */
    return esp_ota_end(ota) == ESP_OK;
}

static void update_task(void *arg)
{
    static Manifest manifest;
    (void)arg;

    log_heap("update mode");
    ESP_LOGI(TAG, "running %s, announced %s", OTAVERSION, target_version[0] ? target_version : "(manual check)");
    set_status("Connecting to WiFi...");
    if (!wait_for_wifi()) finish("No WiFi", true, false);

    set_status("Checking for update...");
    char *buffer = malloc(MANIFEST_MAX);
    if (buffer == NULL) finish("Out of memory", true, false);
    bool manifest_ok = download_manifest(buffer, MANIFEST_MAX) && parse_manifest(buffer, &manifest);
    free(buffer);
    if (!manifest_ok) finish("Update check failed", true, false);

    if (version_compare(manifest.version, OTAVERSION) <= 0) {
        /* Up to date. Only a failure if a newer version was announced. */
        finish("Up to date", target_version[0] != '\0', false);
    }

    const esp_partition_t *slot = esp_ota_get_next_update_partition(NULL);
    if (slot == NULL || manifest.size > (long)slot->size) finish("No room for the update", true, false);
    ESP_LOGI(TAG, "installing %s into %s at 0x%x", manifest.version, slot->label, slot->address);
    if (!install_firmware(slot, &manifest)) finish("Update failed", true, false);

    /* The trial must be recorded before the new image can boot. */
    if (!start_trial(manifest.version)) finish("Update failed", true, false);
    if (esp_ota_set_boot_partition(slot) != ESP_OK) {
        clear_trial();
        finish("Update failed", true, false);
    }
    update_screen_set_progress(100);
    finish("Update installed, restarting", false, true);
}

void ota_update_mode_start(void)
{
    update_screen_init();
    xTaskCreate(update_task, "ota_update", UPDATE_TASK_STACK, NULL, UPDATE_TASK_PRIO, NULL);
}
