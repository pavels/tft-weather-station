#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "City.h"
#include "HttpClient.h"
#include "JsonFeed.h"

#define GEOCODING_HOST "geocoding-api.open-meteo.com"

/* Returns the city as the top-level object. */
#define RESOLVE_PATH_FORMAT "/v1/get?id=%s&language=%s"

typedef struct {
	JsonFeed feed;
	City* city;
	bool has_id;
	bool has_coordinates;
} CityRequest;

static void copy_text(char* dest, size_t size, const char* src) {
	strncpy(dest, src, size - 1);
	dest[size - 1] = '\0';
}

static void on_value(const JsonValue* v, void* ctx) {
	CityRequest* request = ctx;
	City* city = request->city;

	if (strcmp(v->section, "") != 0) return;

	if (strcmp(v->key, "id") == 0) {
		copy_text(city->id, sizeof(city->id), v->text);
		request->has_id = true;
	} else if (strcmp(v->key, "name") == 0) {
		copy_text(city->name, sizeof(city->name), v->text);
	} else if (strcmp(v->key, "admin1") == 0) {
		copy_text(city->region, sizeof(city->region), v->text);
	} else if (strcmp(v->key, "country") == 0) {
		copy_text(city->country, sizeof(city->country), v->text);
	} else if (strcmp(v->key, "latitude") == 0) {
		city->latitude = strtof(v->text, NULL);
	} else if (strcmp(v->key, "longitude") == 0) {
		city->longitude = strtof(v->text, NULL);
		request->has_coordinates = true;
	}
}

static void on_body(const char* data, int length, void* ctx) {
	json_feed_data(&((CityRequest*)ctx)->feed, data, length);
}

bool city_resolve(const char* id, LANGUAGE lang, City* city) {
	static CityRequest request;
	static City result;
	static char path[96];

	memset(&result, 0, sizeof(result));
	request.city = &result;
	request.has_id = false;
	request.has_coordinates = false;
	json_feed_init(&request.feed, on_value, &request);

	snprintf(path, sizeof(path), RESOLVE_PATH_FORMAT, id, lang_strings[STR_LANG_CODE][lang]);
	if (http_get(GEOCODING_HOST, path, on_body, &request) != 1) return false;
	if (!request.has_id || !request.has_coordinates) return false;

	result.valid = true;
	*city = result;
	return true;
}
