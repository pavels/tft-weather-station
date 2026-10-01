#include <stdio.h>
#include <string.h>

#if(TARGET_ESP8266 == 1)
#include "ili9341_driver.h"
#endif

#include "Controller.h"
#include "IconFs.h"

/* Timer periods, ms. */
#define CLOCK_UPDATE_MS   5000           /* clock and indoor readings on the title screen */
#define WEATHER_UPDATE_MS (20 * 60000)   /* weather data refresh: every 20 minutes */
#define POLL_MS           1000           /* WiFi state, backlight schedule, idle screen return */
#define WEATHER_RETRY_MS  60000          /* after a failed refresh, retry every minute */

/* Firmware update check: first a couple of minutes after boot, then hourly. */
#define UPDATE_CHECK_FIRST_MS (2 * 60000)
#define UPDATE_CHECK_MS       (60 * 60000)

/* How long the splash screen waits for the first SNTP sync after WiFi is up,
 * ms. The title screen picks its forecast slots from the current time; after
 * this it shows up anyway (SNTP blocked?), with the clock blank until synced. */
#define CLOCK_WAIT_MS 30000

/* After a tap wakes the dimmed screen, it dims again after this much inactivity, ms. */
#define AWAKE_TIMEOUT_MS 5000
/* The forecast screen returns to the title screen after this much inactivity, ms. */
#define FORECAST_IDLE_MS 10000
/* Up to this "off" brightness (0-255), a tap on the dimmed screen only wakes it
 * up; above it the screen is readable, so taps act on the UI directly. */
#define SLEEP_WAKE_MAX_BRIGHTNESS 127

WeatherStationConfig weather_station_config;

bool wifi_connected = false;

float local_temperature = 0;
float local_humidity = 0;

lv_obj_t* title_screen_container;
lv_obj_t* forecast_screen_container;
lv_obj_t* config_screen_container;
lv_obj_t* splash_screen_container;

VisibleScreenType visible_screen;
BacklightModeType backlight_mode;

City current_city;
WeatherData weather;

static lv_timer_t* data_timer;

static void title_event_cb(lv_event_t *e);
static void forecast_event_cb(lv_event_t *e);
static bool process_sleep_click(void);

/* Hourly firmware update check, which may restart into update mode: only from
 * the weather screens, never while someone is in the settings. */
static void update_checker(lv_timer_t *timer) {
	lv_timer_set_period(timer, UPDATE_CHECK_MS);
	if(!wifi_connected) return;
	if(visible_screen != VIS_SCREEN_TITLE && visible_screen != VIS_SCREEN_FORECAST) return;
	platform_check_for_update(backlight_mode == BACKLIGHT_SLEEP);
}

static void time_updater(lv_timer_t *timer) {
	(void)timer;
	set_time();
	set_local_cond();
}

/* Loads the data and schedules the next refresh: the regular period, or a
 * quick retry after a failure. */
static void refresh_data(void) {
	bool ok = load_remote_data();
	if(ok) platform_confirm_boot();
	lv_timer_set_period(data_timer, ok ? WEATHER_UPDATE_MS : WEATHER_RETRY_MS);
	lv_timer_reset(data_timer);
}

static void data_updater(lv_timer_t *timer) {
	(void)timer;
	if(wifi_connected && visible_screen != VIS_SCREEN_SETTINGS) {
		refresh_data();

		if(visible_screen == VIS_SCREEN_TITLE) {
			destroy_title_screen(title_screen_container);
			display_title_screen();
		}else if (visible_screen == VIS_SCREEN_FORECAST){
			destroy_forecast_screen(forecast_screen_container);
			display_forecast_screen();
		}
	}
}

/* True once the clock is set, or the splash has waited CLOCK_WAIT_MS for it. */
static bool clock_wait_over(void) {
	static bool waiting;
	static uint32_t wait_start;

	if(clock_is_set()) return true;
	if(!waiting) {
		waiting = true;
		wait_start = lv_tick_get();
	}
	return lv_tick_elaps(wait_start) > CLOCK_WAIT_MS;
}

static void wifi_check(lv_timer_t *timer) {
	(void)timer;
	if(wifi_connected && visible_screen == VIS_SCREEN_SPLASH && clock_wait_over()){
		refresh_data();
		lv_obj_delete(splash_screen_container);
		display_title_screen();
		set_backlight_level(weather_station_config.power_on_brightness);
	}

	update_wifi_status();
}

static void backlight_updater(lv_timer_t *timer) {
	(void)timer;
	uint32_t timestamp_power_off = weather_station_config.power_off_hour * 3600 +  weather_station_config.power_off_minute * 60;
	uint32_t timestamp_power_on = weather_station_config.power_on_hour * 3600 +  weather_station_config.power_on_minute * 60;
	uint32_t current_time_part = (uint32_t)get_current_time() % 86400; // Only time of day

	bool power_off = false;

	if(visible_screen != VIS_SCREEN_TITLE && visible_screen != VIS_SCREEN_FORECAST) return;

	if(timestamp_power_off > timestamp_power_on){
		power_off = (current_time_part < timestamp_power_on || current_time_part > timestamp_power_off);
	}else if(timestamp_power_off < timestamp_power_on) {
		power_off = (current_time_part > timestamp_power_off && current_time_part < timestamp_power_on);
	}

	if(power_off == false && backlight_mode == BACKLIGHT_ON) return;

	if(power_off == false && backlight_mode != BACKLIGHT_ON) {
		backlight_mode = BACKLIGHT_ON;
		set_backlight_level(weather_station_config.power_on_brightness);
		return;
	}

	// Going off
	if(backlight_mode == BACKLIGHT_ON) {
		backlight_mode = BACKLIGHT_SLEEP;
		set_backlight_level(weather_station_config.power_off_brightness);
		return;
	}

	// Awake timeout
	if(backlight_mode == BACKLIGHT_AWAKE && lv_display_get_inactive_time(NULL) > AWAKE_TIMEOUT_MS) {
		backlight_mode = BACKLIGHT_SLEEP;
		set_backlight_level(weather_station_config.power_off_brightness);
	}
}

static void screen_updater(lv_timer_t *timer) {
	(void)timer;
	if(visible_screen == VIS_SCREEN_FORECAST && lv_display_get_inactive_time(NULL) > FORECAST_IDLE_MS){
		destroy_forecast_screen(forecast_screen_container);
		display_title_screen();
	}
}

void initialize_tft_station(void){
	custom_styles_init();
	icon_fs_init();

	load_ws_config(&weather_station_config);

	lv_timer_create(time_updater, CLOCK_UPDATE_MS, NULL);
	data_timer = lv_timer_create(data_updater, WEATHER_UPDATE_MS, NULL);
	lv_timer_create(wifi_check, POLL_MS, NULL);
	lv_timer_create(backlight_updater, POLL_MS, NULL);
	lv_timer_create(screen_updater, POLL_MS, NULL);
	lv_timer_create(update_checker, UPDATE_CHECK_FIRST_MS, NULL);

	visible_screen = VIS_SCREEN_SPLASH;
	backlight_mode = BACKLIGHT_ON;

	splash_screen_container = create_splash_screen(lv_screen_active());
}

bool load_remote_data(void) {
	if(!current_city.valid || strcmp(current_city.id, weather_station_config.city_id) != 0) {
		if(!city_resolve(weather_station_config.city_id, weather_station_config.language, &current_city)) return false;
	}
	if(!weather_load(current_city.latitude, current_city.longitude, &weather)) return false;
	clock_set_utc_offset(weather.utc_offset);
	return true;
}

void display_title_screen(void){
	visible_screen = VIS_SCREEN_TITLE;
	title_screen_container = create_title_screen(lv_screen_active(), &current_city, &weather);
	lv_obj_set_clickable(title_screen_container, true);
	lv_obj_add_event_cb(title_screen_container, title_event_cb, LV_EVENT_CLICKED, NULL);
}

void display_forecast_screen(void){
	visible_screen = VIS_SCREEN_FORECAST;
	forecast_screen_container = create_forecast_screen(lv_screen_active(), &weather);
	lv_obj_set_clickable(forecast_screen_container, true);
	lv_obj_add_event_cb(forecast_screen_container, forecast_event_cb, LV_EVENT_CLICKED, NULL);
}

void display_config_screen(void){
	visible_screen = VIS_SCREEN_SETTINGS;
	set_backlight_level(weather_station_config.power_on_brightness);
	config_screen_container = create_config_screen(lv_screen_active());
}

static void title_event_cb(lv_event_t *e){
	(void)e;
	if(visible_screen != VIS_SCREEN_TITLE) return;
	if(process_sleep_click()) return;

	destroy_title_screen(title_screen_container);
	display_forecast_screen();
}

static void forecast_event_cb(lv_event_t *e){
	(void)e;
	if(visible_screen != VIS_SCREEN_FORECAST) return;
	if(process_sleep_click()) return;

	destroy_forecast_screen(forecast_screen_container);
	display_title_screen();
}

void settings_click_action(lv_event_t *e) {
	(void)e;
	if(visible_screen == VIS_SCREEN_TITLE) {
		destroy_title_screen(title_screen_container);
	}else if(visible_screen == VIS_SCREEN_SPLASH){
		lv_obj_delete(splash_screen_container);
	}
	display_config_screen();
}

static bool process_sleep_click(void){
	if(weather_station_config.power_off_brightness > SLEEP_WAKE_MAX_BRIGHTNESS) return false;

	if(backlight_mode == BACKLIGHT_SLEEP) {
		backlight_mode = BACKLIGHT_AWAKE;
		set_backlight_level(weather_station_config.power_on_brightness);
		return true;
	}

	return false;
}

void set_backlight_level(int32_t brightness) {
	#if(TARGET_ESP8266 == 1)
		ili9341_set_backlight(brightness);
	#else
		(void)brightness;
	#endif
}
