#ifndef APP_CONTROLLER_H_
#define APP_CONTROLLER_H_

#include <stdbool.h>

#include "CustomStyles.h"
#include "MainScreens.h"
#include "SplashScreen.h"
#include "ConfigScreen.h"
#include "City.h"
#include "Clock.h"
#include "Config.h"
#include "Weather.h"

typedef enum {
	VIS_SCREEN_TITLE,
	VIS_SCREEN_FORECAST,
	VIS_SCREEN_SETTINGS,
	VIS_SCREEN_SPLASH,
	VIS_SCREEN_UPDATE
} VisibleScreenType;

typedef enum {
	BACKLIGHT_ON,
	BACKLIGHT_AWAKE,
	BACKLIGHT_SLEEP
} BacklightModeType;

extern WeatherStationConfig weather_station_config;

/* Set by the platform entry point: app_main.c's WiFi event handler, or always
 * true in the simulator. */
extern bool wifi_connected;

extern float local_temperature;
extern float local_humidity;

extern lv_obj_t* title_screen_container;
extern lv_obj_t* forecast_screen_container;
extern lv_obj_t* config_screen_container;
extern lv_obj_t* splash_screen_container;

extern VisibleScreenType visible_screen;
extern BacklightModeType backlight_mode;

/* The configured city, resolved to name and coordinates. */
extern City current_city;
extern WeatherData weather;

void initialize_tft_station(void);

/* Resolves the city if needed, then loads the weather. False on failure. */
bool load_remote_data(void);

void display_title_screen(void);
void display_forecast_screen(void);
void display_config_screen(void);

void set_backlight_level(int32_t brightness);

/* Firmware updates, implemented by the platform entry point (app_main.c /
 * simulator/main.c). The hourly check restarts into update mode if a newer
 * release exists; `power_off_hours` allows its fallback when the version
 * service has been unreachable for days. A manual check ("Check for updates")
 * always restarts into update mode, which installs the latest release if it
 * is newer. */
void platform_check_for_update(bool power_off_hours);
void platform_manual_update(void);

/* The app works on this platform (weather loaded): a freshly updated image
 * ends its trial and stays. Implemented by the platform entry point. */
void platform_confirm_boot(void);

/* Wired directly to the settings button in SplashScreen.c/MainScreens.c. */
void settings_click_action(lv_event_t *e);

#endif /* APP_CONTROLLER_H_ */
