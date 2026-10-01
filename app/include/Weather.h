#ifndef APP_WEATHER_H_
#define APP_WEATHER_H_

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Hourly forecast length: 4 days from today 00:00 local, which covers the
 * forecast screen's last slot (day+3 22:00). */
#define FORECAST_HOURS (4 * 24)

typedef struct {
	bool valid;
	float temp;           /* degC */
	uint8_t weather_code; /* WMO code */
} CurrentWeather;

typedef struct {
	time_t time;          /* local time as UTC (see Clock.h) */
	float temp;           /* degC */
	float rain;           /* mm in the preceding hour */
	uint8_t weather_code; /* WMO code */
} HourlyForecast;

typedef struct {
	CurrentWeather current;
	HourlyForecast hourly[FORECAST_HOURS];
	int hourly_count;
	int32_t utc_offset; /* seconds, DST included */
} WeatherData;

/* Loads current conditions and the hourly forecast for the given coordinates
 * from Open-Meteo. Returns false if the request failed or returned no data. */
bool weather_load(float latitude, float longitude, WeatherData* data);

/* The forecast hour closest to `local_time`, or NULL without forecast data. */
const HourlyForecast* weather_forecast_at(const WeatherData* data, time_t local_time);

#endif /* APP_WEATHER_H_ */
