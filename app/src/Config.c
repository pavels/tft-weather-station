#include <string.h>

#include "Config.h"
#include "credentials.h"

#if(TARGET_ESP8266 == 1)
#include "esp_log.h"
#include "nvs.h"
#endif

/*
 * Defaults, overridden field by field by whatever is saved in NVS (ESP8266
 * only). nvs_flash_init() is done by app_main.c, since WiFi needs NVS too.
 * WIFI_SSID / WIFI_PASSWORD from credentials.h are an optional bench-only
 * default for boards with no WiFi config in NVS.
 */

/* Defaults for a device with nothing saved. */
#define DEFAULT_CITY_ID          "3067696" /* GeoNames id: Praha */
#define DEFAULT_POWER_ON_HOUR    6         /* full brightness from 6:00... */
#define DEFAULT_POWER_OFF_HOUR   22        /* ...until 22:00 */
#define DEFAULT_ON_BRIGHTNESS    255       /* 0-255 */
#define DEFAULT_OFF_BRIGHTNESS   63        /* dimmed at night, still readable */

#if(TARGET_ESP8266 == 1)
static const char *TAG = "config";

/* Where the config lives in NVS. Namespace and keys must stay stable across
 * firmware versions: devices keep their saved settings under them. NVS keys
 * are limited to 15 characters, hence the short names. */
#define NVS_NAMESPACE "ws_config"
#define KEY_WIFI_SSID     "wifi_ssid"
#define KEY_WIFI_PASSWORD "wifi_password"
#define KEY_CITY_ID       "city_id"
#define KEY_ON_HOUR       "pwr_on_hour"
#define KEY_ON_MINUTE     "pwr_on_min"
#define KEY_OFF_HOUR      "pwr_off_hour"
#define KEY_OFF_MINUTE    "pwr_off_min"
#define KEY_ON_BRIGHT     "bright_on"
#define KEY_OFF_BRIGHT    "bright_off"
#define KEY_LANGUAGE      "language"

/* Leaves `value` untouched (i.e. the default) when the key isn't stored. */
static void load_string(nvs_handle handle, const char *key, char *value, size_t size) {
	size_t len = size;
	esp_err_t err = nvs_get_str(handle, key, value, &len);
	if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGW(TAG, "reading %s failed: %s", key, esp_err_to_name(err));
	}
}

static void load_int(nvs_handle handle, const char *key, int32_t *value) {
	esp_err_t err = nvs_get_i32(handle, key, value);
	if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGW(TAG, "reading %s failed: %s", key, esp_err_to_name(err));
	}
}
#endif

void load_ws_config(WeatherStationConfig* target_config) {
#ifdef WIFI_SSID
	strncpy(target_config->wifi_ssid, WIFI_SSID, sizeof(target_config->wifi_ssid) - 1);
	target_config->wifi_ssid[sizeof(target_config->wifi_ssid) - 1] = '\0';
#else
	target_config->wifi_ssid[0] = '\0';
#endif
#ifdef WIFI_PASSWORD
	strncpy(target_config->wifi_password, WIFI_PASSWORD, sizeof(target_config->wifi_password) - 1);
	target_config->wifi_password[sizeof(target_config->wifi_password) - 1] = '\0';
#else
	target_config->wifi_password[0] = '\0';
#endif
	strcpy(target_config->city_id, DEFAULT_CITY_ID);

	target_config->power_on_hour = DEFAULT_POWER_ON_HOUR;
	target_config->power_on_minute = 0;

	target_config->power_off_hour = DEFAULT_POWER_OFF_HOUR;
	target_config->power_off_minute = 0;

	target_config->power_on_brightness = DEFAULT_ON_BRIGHTNESS;
	target_config->power_off_brightness = DEFAULT_OFF_BRIGHTNESS;

	target_config->language = ENGLISH;

#if(TARGET_ESP8266 == 1)
	nvs_handle handle;
	esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
	if (err == ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGI(TAG, "no saved config, using defaults");
		return;
	}
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nvs_open failed: %s, using defaults", esp_err_to_name(err));
		return;
	}

	load_string(handle, KEY_WIFI_SSID, target_config->wifi_ssid, sizeof(target_config->wifi_ssid));
	load_string(handle, KEY_WIFI_PASSWORD, target_config->wifi_password, sizeof(target_config->wifi_password));
	load_string(handle, KEY_CITY_ID, target_config->city_id, sizeof(target_config->city_id));

	load_int(handle, KEY_ON_HOUR, &target_config->power_on_hour);
	load_int(handle, KEY_ON_MINUTE, &target_config->power_on_minute);
	load_int(handle, KEY_OFF_HOUR, &target_config->power_off_hour);
	load_int(handle, KEY_OFF_MINUTE, &target_config->power_off_minute);
	load_int(handle, KEY_ON_BRIGHT, &target_config->power_on_brightness);
	load_int(handle, KEY_OFF_BRIGHT, &target_config->power_off_brightness);

	int32_t language = target_config->language;
	load_int(handle, KEY_LANGUAGE, &language);
	target_config->language = (LANGUAGE)language;

	nvs_close(handle);
#endif
}

void store_ws_config(WeatherStationConfig* source_config) {
#if(TARGET_ESP8266 == 1)
	nvs_handle handle;
	esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nvs_open failed: %s, config not saved", esp_err_to_name(err));
		return;
	}

	err = nvs_set_str(handle, KEY_WIFI_SSID, source_config->wifi_ssid);
	if (err == ESP_OK) err = nvs_set_str(handle, KEY_WIFI_PASSWORD, source_config->wifi_password);
	if (err == ESP_OK) err = nvs_set_str(handle, KEY_CITY_ID, source_config->city_id);
	if (err == ESP_OK) err = nvs_set_i32(handle, KEY_ON_HOUR, source_config->power_on_hour);
	if (err == ESP_OK) err = nvs_set_i32(handle, KEY_ON_MINUTE, source_config->power_on_minute);
	if (err == ESP_OK) err = nvs_set_i32(handle, KEY_OFF_HOUR, source_config->power_off_hour);
	if (err == ESP_OK) err = nvs_set_i32(handle, KEY_OFF_MINUTE, source_config->power_off_minute);
	if (err == ESP_OK) err = nvs_set_i32(handle, KEY_ON_BRIGHT, source_config->power_on_brightness);
	if (err == ESP_OK) err = nvs_set_i32(handle, KEY_OFF_BRIGHT, source_config->power_off_brightness);
	if (err == ESP_OK) err = nvs_set_i32(handle, KEY_LANGUAGE, source_config->language);
	if (err == ESP_OK) err = nvs_commit(handle);

	if (err != ESP_OK) {
		ESP_LOGE(TAG, "saving config failed: %s", esp_err_to_name(err));
	}
	nvs_close(handle);
#else
	(void)source_config;
#endif
}
