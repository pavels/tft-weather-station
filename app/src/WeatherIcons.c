#include "WeatherIcons.h"

/* Icons are LVGL file paths on the "I:" drive (IconFs.c), served from the
 * icons.bin blob -- not compiled in, to keep the ESP8266 app image small. */
#define ICON(name) { "I:wi_" name "_big.bin", "I:wi_" name "_small.bin" }

typedef struct {
	const char* big;
	const char* small;
} IconPair;

/* WMO weather code -> icon. The set has no night variants. "chance*" icons
 * stand for the light/slight intensities. */
static const struct {
	uint8_t code;
	IconPair icon;
} WEATHER_ICONS[] = {
	{  0, ICON("sunny") },
	{  1, ICON("mostlysunny") },
	{  2, ICON("partlycloudy") },
	{  3, ICON("cloudy") },
	{ 45, ICON("fog") },
	{ 48, ICON("fog") },
	{ 51, ICON("chancerain") },
	{ 53, ICON("chancerain") },
	{ 55, ICON("rain") },
	{ 56, ICON("chancesleet") },
	{ 57, ICON("sleet") },
	{ 61, ICON("chancerain") },
	{ 63, ICON("rain") },
	{ 65, ICON("rain") },
	{ 66, ICON("sleet") },
	{ 67, ICON("sleet") },
	{ 71, ICON("chanceflurries") },
	{ 73, ICON("flurries") },
	{ 75, ICON("snow") },
	{ 77, ICON("flurries") },
	{ 80, ICON("chancerain") },
	{ 81, ICON("rain") },
	{ 82, ICON("rain") },
	{ 85, ICON("chancesnow") },
	{ 86, ICON("snow") },
	{ 95, ICON("tstorms") },
	{ 96, ICON("tstorms") },
	{ 99, ICON("tstorms") },
};

static const IconPair UNKNOWN_ICON = ICON("unknown");

static const IconPair* find_icon(uint8_t code) {
	for (unsigned i = 0; i < sizeof(WEATHER_ICONS) / sizeof(WEATHER_ICONS[0]); i++) {
		if (WEATHER_ICONS[i].code == code) return &WEATHER_ICONS[i].icon;
	}
	return &UNKNOWN_ICON;
}

const char* weather_icon_big(uint8_t code) {
	return find_icon(code)->big;
}

const char* weather_icon_small(uint8_t code) {
	return find_icon(code)->small;
}
