#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Weather.h"
#include "HttpClient.h"
#include "JsonFeed.h"

#define OPEN_METEO_HOST "api.open-meteo.com"

/* Only the fields the screens use; times as unix timestamps (UTC), and
 * timezone=auto for the location's utc_offset_seconds. ~2.8KB response. */
#define FORECAST_PATH_FORMAT "/v1/forecast?latitude=%.4f&longitude=%.4f" \
	"&current=temperature_2m,weather_code" \
	"&hourly=temperature_2m,precipitation,weather_code" \
	"&forecast_days=4&timezone=auto&timeformat=unixtime"

static void on_value(const JsonValue* v, void* ctx) {
	WeatherData* data = ctx;

	if (strcmp(v->section, "") == 0) {
		if (strcmp(v->key, "utc_offset_seconds") == 0) data->utc_offset = strtol(v->text, NULL, 10);
	} else if (strcmp(v->section, "current") == 0) {
		if (strcmp(v->key, "temperature_2m") == 0) {
			data->current.temp = strtof(v->text, NULL);
			data->current.valid = true;
		} else if (strcmp(v->key, "weather_code") == 0) {
			data->current.weather_code = strtol(v->text, NULL, 10);
		}
	} else if (strcmp(v->section, "hourly") == 0) {
		if (v->index < 0 || v->index >= FORECAST_HOURS) return;
		HourlyForecast* hour = &data->hourly[v->index];
		if (strcmp(v->key, "time") == 0) {
			hour->time = strtol(v->text, NULL, 10);
			if (v->index >= data->hourly_count) data->hourly_count = v->index + 1;
		} else if (strcmp(v->key, "temperature_2m") == 0) {
			hour->temp = strtof(v->text, NULL);
		} else if (strcmp(v->key, "precipitation") == 0) {
			hour->rain = strtof(v->text, NULL);
		} else if (strcmp(v->key, "weather_code") == 0) {
			hour->weather_code = strtol(v->text, NULL, 10);
		}
	}
}

typedef struct {
	JsonFeed feed;
	WeatherData* data;
	bool started;
} WeatherRequest;

static void on_body(const char* data, int length, void* ctx) {
	WeatherRequest* request = ctx;
	/* Cleared only once a response arrives: a failed connection keeps the
	 * previous data on screen. */
	if (!request->started) {
		memset(request->data, 0, sizeof(*request->data));
		request->started = true;
	}
	json_feed_data(&request->feed, data, length);
}

bool weather_load(float latitude, float longitude, WeatherData* data) {
	static WeatherRequest request;
	static char path[256];

	request.data = data;
	request.started = false;
	json_feed_init(&request.feed, on_value, data);

	snprintf(path, sizeof(path), FORECAST_PATH_FORMAT, latitude, longitude);
	if (http_get(OPEN_METEO_HOST, path, on_body, &request) != 1) return false;

	for (int i = 0; i < data->hourly_count; i++) data->hourly[i].time += data->utc_offset;
	return data->current.valid && data->hourly_count > 0;
}

const HourlyForecast* weather_forecast_at(const WeatherData* data, time_t local_time) {
	if (data->hourly_count == 0) return NULL;

	/* Hourly steps: round to the nearest hour, clamped to the forecast range. */
	long index = (long)(local_time - data->hourly[0].time + 1800) / 3600;
	if (index < 0) index = 0;
	if (index >= data->hourly_count) index = data->hourly_count - 1;
	return &data->hourly[index];
}
