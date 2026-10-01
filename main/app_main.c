#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "esp_sntp.h"

#include "lvgl.h"
#include "ili9341_driver.h"
#include "spi_bus.h"
#include "xpt2046_driver.h"
#include "dht_driver.h"
#include "Controller.h"
#include "ota_update.h"

#include "../version.h"

static const char *TAG = "app_main";

/* LVGL partial render buffer height, in panel lines: 240 x 24 x 2 bytes =
 * 11.5KB of DRAM. More lines mean fewer flushes per frame but less heap. */
#define LV_BUF_LINES 24

/* How often heap_log_task logs heap and stack usage. */
#define HEAP_LOG_PERIOD_MS 2000

/* How often the LVGL task runs lv_timer_handler(), ms: one RTOS tick. */
#define LVGL_TASK_PERIOD_MS 10

/* Task stacks (bytes on this port) and priorities (higher runs first). The
 * DHT task sits above LVGL so the sensor's start pulse isn't stretched past
 * spec while LVGL renders; the heap log is background. LVGL's draw path is
 * stack-hungry: 4096 leaves under 300 bytes free on the splash screen; the
 * heap log's "stack free: lvgl" tracks this. */
#define HEAP_LOG_STACK 2048
#define HEAP_LOG_PRIO  1
#define LVGL_STACK     6144
#define LVGL_PRIO      5
#define DHT_STACK      2048
#define DHT_PRIO       6

static TaskHandle_t lvgl_task_handle;

static const char *screen_name(VisibleScreenType s)
{
    switch (s) {
    case VIS_SCREEN_SPLASH: return "splash";
    case VIS_SCREEN_TITLE: return "title";
    case VIS_SCREEN_FORECAST: return "forecast";
    case VIS_SCREEN_SETTINGS: return "config";
    case VIS_SCREEN_UPDATE: return "update";
    }
    return "?";
}

/* Own task, not an lv_timer, so it keeps logging even if LVGL stalls. Stack
 * watermarks are in bytes on this port (portSTACK_TYPE is uint8_t). The two
 * heaps are DRAM (8-bit accessible) and the IRAM heap (32-bit access only);
 * plain malloc() can be served from either. */
static void heap_log_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "heap: free %u (8bit %u), min ever %u (8bit %u) | stack free: lvgl %u, heap_log %u | screen %s",
                 heap_caps_get_free_size(MALLOC_CAP_32BIT), heap_caps_get_free_size(MALLOC_CAP_8BIT),
                 heap_caps_get_minimum_free_size(MALLOC_CAP_32BIT), heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT),
                 lvgl_task_handle ? (unsigned)uxTaskGetStackHighWaterMark(lvgl_task_handle) : 0,
                 (unsigned)uxTaskGetStackHighWaterMark(NULL), screen_name(visible_screen));
        vTaskDelay(pdMS_TO_TICKS(HEAP_LOG_PERIOD_MS));
    }
}

/* LVGL's millisecond clock, read on demand. Not a periodic esp_timer calling
 * lv_tick_inc(): on this SDK esp_timer rejects any period that isn't a whole
 * number of RTOS ticks (10ms), leaving LVGL's clock at 0. 10ms resolution is
 * plenty for a ~33ms refresh period. */
static uint32_t lv_tick_get_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Unusable partition (truncated, or written by a newer NVS format):
         * the standard SDK recovery is to wipe it and start over. */
        ESP_LOGW(TAG, "NVS unusable (%s), erasing", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

/* NTP server for the system clock; lwIP re-syncs every
 * CONFIG_LWIP_SNTP_UPDATE_DELAY (1 hour by default). */
#define NTP_SERVER "pool.ntp.org"

/* Started on the first IP, so the first request goes out right away; it keeps
 * running across WiFi reconnects. */
static void on_time_sync(struct timeval *tv)
{
    ESP_LOGI(TAG, "sntp: clock set, %ld UTC", (long)tv->tv_sec);
}

static void sntp_start(void)
{
    if (sntp_enabled()) return;
    sntp_set_time_sync_notification_cb(on_time_sync);
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, NTP_SERVER);
    sntp_init();
}

/* Runs in the default event loop task. Controller.c polls wifi_connected
 * (wifi_check) to leave the splash screen and to color the WiFi icon. */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        wifi_connected = false;
        /* Retry forever: units must recover from router reboots by
         * themselves. Each failed attempt takes seconds, so no backoff. */
        ESP_LOGW(TAG, "wifi: disconnected (reason %d), reconnecting", event->reason);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "wifi: connected, ip " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
        sntp_start();
    }
}

static void wifi_start(void)
{
    if (weather_station_config.wifi_ssid[0] == '\0') {
        ESP_LOGW(TAG, "wifi: no SSID configured (NVS empty, no WIFI_SSID in credentials.h), not starting");
        return;
    }

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    /* Config.c's NVS namespace is the single source of truth for credentials;
     * don't let the WiFi driver keep its own second copy in NVS. */
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_cfg = {0};
    /* Both source strings are NUL-terminated and fit (31/63 chars max). */
    strncpy((char *)wifi_cfg.sta.ssid, weather_station_config.wifi_ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy((char *)wifi_cfg.sta.password, weather_station_config.wifi_password, sizeof(wifi_cfg.sta.password));
    /* PMF stays off: WPA2 only (WPA3 is disabled in sdkconfig.defaults). The
     * first association after a reset often fails (reason 203, then 205) and
     * succeeds on retry, covered by the loop in wifi_event_handler. */

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi: connecting to \"%s\"", weather_station_config.wifi_ssid);
}

/* Indoor readings every 2 minutes; room temperature doesn't change faster. */
#define DHT_PERIOD_MS 120000
#define DHT_RETRY_MS 10000        /* after a failed read... */
#define DHT_FAST_RETRIES 3        /* ...this many times, then back to DHT_PERIOD_MS */
#define DHT_FIRST_READ_MS 2000    /* sensor needs ~1-2s after power-up */

/* Indoor temperature/humidity for the title screen (local_temperature /
 * local_humidity, shown by Controller.c's time_updater). */
static void dht_task(void *pvParameters)
{
    int failures = 0;

    dht22_init();
    vTaskDelay(pdMS_TO_TICKS(DHT_FIRST_READ_MS));
    while (1) {
        int16_t humidity, temperature;
        int failed_bit;
        dht_result_t result = dht22_read(&humidity, &temperature, &failed_bit);
        if (result == DHT_OK) {
            local_temperature = temperature / 10.0f;
            local_humidity = humidity / 10.0f;
            ESP_LOGI(TAG, "dht22: %.1f C, %.1f %%", local_temperature, local_humidity);
            failures = 0;
        } else {
            failures++;
            /* Logged while retrying fast; a missing sensor then settles to
             * one line per DHT_PERIOD_MS. */
            ESP_LOGW(TAG, "dht22: read failed: %s (bit %d), attempt %d", dht22_result_name(result),
                     failed_bit, failures);
            if (result == DHT_ERR_CHECKSUM) {
                const dht_capture_t *c = dht22_last_capture();
                char low[sizeof(c->low) * 3 + 1], high[sizeof(c->high) * 3 + 1];
                /* Per-bit durations in microseconds, space-separated. */
                for (size_t i = 0; i < sizeof(c->low); i++) {
                    snprintf(low + i * 3, 4, "%2u ", c->low[i] > 99 ? 99 : c->low[i]);
                    snprintf(high + i * 3, 4, "%2u ", c->high[i] > 99 ? 99 : c->high[i]);
                }
                ESP_LOGW(TAG, "dht22: bytes %02x %02x %02x %02x %02x", c->data[0], c->data[1], c->data[2],
                         c->data[3], c->data[4]);
                ESP_LOGW(TAG, "dht22: low us  %s", low);
                ESP_LOGW(TAG, "dht22: high us %s", high);
            }
        }
        bool retry_fast = result != DHT_OK && failures <= DHT_FAST_RETRIES;
        vTaskDelay(pdMS_TO_TICKS(retry_fast ? DHT_RETRY_MS : DHT_PERIOD_MS));
    }
}

/* Diagnostic: logs frames slower than FRAME_LOG_MIN_MS, split into time spent
 * in the flush callback (SPI transfer to the panel) and everything else
 * (LVGL rendering, incl. fonts and icon reads). */
#define FRAME_TIMING_LOG 0   /* 1 = enabled */
#define FRAME_LOG_MIN_MS 100 /* only frames slower than this are logged */

#if FRAME_TIMING_LOG
static int64_t frame_start_us, frame_flush_us;
static uint32_t frame_flushes, frame_pixels;

static void timed_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int64_t t0 = esp_timer_get_time();
    ili9341_flush_cb(disp, area, px_map);
    frame_flush_us += esp_timer_get_time() - t0;
    frame_flushes++;
    frame_pixels += lv_area_get_size(area);
}

static void frame_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_REFR_START) {
        frame_start_us = esp_timer_get_time();
        frame_flush_us = 0;
        frame_flushes = frame_pixels = 0;
        return;
    }
    int64_t total_us = esp_timer_get_time() - frame_start_us;
    if (frame_flushes > 0 && total_us >= FRAME_LOG_MIN_MS * 1000) {
        ESP_LOGI(TAG, "frame: %d ms total = %d ms render + %d ms flush (%u flushes, %u px) | screen %s",
                 (int)(total_us / 1000), (int)((total_us - frame_flush_us) / 1000), (int)(frame_flush_us / 1000),
                 frame_flushes, frame_pixels, screen_name(visible_screen));
    }
}
#endif

static void lvgl_task(void *pvParameters)
{
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(LVGL_TASK_PERIOD_MS));
    }
}

/* LVGL, the weather UI and the indoor sensor. */
static void start_ui(void)
{
    lv_init();
    lv_tick_set_cb(lv_tick_get_ms);

    lv_display_t *disp = lv_display_create(ILI9341_HOR_RES, ILI9341_VER_RES);
#if FRAME_TIMING_LOG
    lv_display_set_flush_cb(disp, timed_flush_cb);
    lv_display_add_event_cb(disp, frame_event_cb, LV_EVENT_REFR_START, NULL);
    lv_display_add_event_cb(disp, frame_event_cb, LV_EVENT_REFR_READY, NULL);
#else
    lv_display_set_flush_cb(disp, ili9341_flush_cb);
#endif
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    /* Heap, not static: update mode doesn't use it, and needs the RAM for TLS. */
    size_t disp_buf_size = ILI9341_HOR_RES * LV_BUF_LINES * 2; /* RGB565 */
    uint8_t *disp_buf = heap_caps_malloc(disp_buf_size, MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(disp_buf == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    lv_display_set_buffers(disp, disp_buf, NULL, disp_buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    xpt2046_init();
    lv_indev_t *touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, xpt2046_read_cb);

    ESP_LOGI(TAG, "heap before UI: free %u, 8bit %u",
             heap_caps_get_free_size(MALLOC_CAP_32BIT), heap_caps_get_free_size(MALLOC_CAP_8BIT));

    initialize_tft_station();
    wifi_start(); /* needs weather_station_config, loaded just above */
    xTaskCreate(dht_task, "dht", DHT_STACK, NULL, DHT_PRIO, NULL);
    xTaskCreate(lvgl_task, "lvgl_task", LVGL_STACK, NULL, LVGL_PRIO, &lvgl_task_handle);
}

/* No LVGL, weather UI or sensor: the update screen is drawn straight to the
 * panel, and the RAM goes to TLS. */
static void start_update_mode(void)
{
    visible_screen = VIS_SCREEN_UPDATE;
    ESP_LOGI(TAG, "heap before update mode: free %u, 8bit %u",
             heap_caps_get_free_size(MALLOC_CAP_32BIT), heap_caps_get_free_size(MALLOC_CAP_8BIT));
    load_ws_config(&weather_station_config);
    ota_update_mode_start();
    wifi_start();
}

void app_main(void)
{
    ESP_LOGI(TAG, "tft_weather_station %s starting", OTAVERSION);

    nvs_init(); /* before the config is loaded, and for WiFi */
    ota_trial_boot_check(); /* first: a crashing new image must still be counted */

    spi_bus_init(); /* shared by the display and the touch controller */
    ili9341_init();
    ili9341_set_backlight(255); /* full brightness until the config is loaded */

    if (ota_update_requested()) {
        start_update_mode();
    } else {
        start_ui();
    }
    xTaskCreate(heap_log_task, "heap_log", HEAP_LOG_STACK, NULL, HEAP_LOG_PRIO, NULL);
}

void platform_check_for_update(bool power_off_hours)
{
    ota_check_for_update(power_off_hours);
}

void platform_manual_update(void)
{
    ota_request_update(NULL);
}

void platform_confirm_boot(void)
{
    ota_confirm_boot();
}
