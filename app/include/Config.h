#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#include <stdint.h>

#include "Lang.h"

typedef struct WeatherStationConfig {
	char wifi_ssid[32];
	char wifi_password[64];

	char city_id[16];

	int32_t power_on_hour;
	int32_t power_on_minute;

	int32_t power_off_hour;
	int32_t power_off_minute;

	int32_t power_on_brightness;
	int32_t power_off_brightness;
	LANGUAGE language;
} WeatherStationConfig;

void load_ws_config(WeatherStationConfig* target);
void store_ws_config(WeatherStationConfig* source);

#endif /* APP_CONFIG_H_ */
