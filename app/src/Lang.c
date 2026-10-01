#include "Lang.h"

const char* LANGUAGE_NAMES[LAST_LANGUAGE] = {
	"English",
	"Čeština"
};

const char* WDAY_NAMES[LAST_LANGUAGE][7] = {
	{"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"},
	{"NE", "PO", "ÚT", "ST", "ČT", "PÁ", "SO"}
};

const char* MONTH_NAMES[LAST_LANGUAGE][12] = {
	{"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"},
	{"LED", "ÚNO", "BŘE", "DUB", "KVĚ", "ČVN", "ČVC", "SRP", "ZÁŘ", "ŘÍJ", "LIS", "PRO"}
};

const char* lang_strings[LAST_STR][LAST_LANGUAGE] = {
	/* ISO 639-1, as the geocoding API's language parameter takes it. */
	[STR_LANG_CODE] = {
		"en",
		"cs"
	},
	[STR_CONNECTING] = {
		"Connecting...",
		"Navazuji spojení..."
	},
	[STR_INDOOR_TEMP] = {
		"Indoor Temp.",
		"Vnitřní tep."
	},
	[STR_INDOOR_HUMIDITY] = {
		"Indoor Hum.",
		"Vnitřní rel. v."
	}
};

/* WMO weather interpretation codes (WMO 4677 subset used by Open-Meteo). */
static const struct {
	uint8_t code;
	const char* text[LAST_LANGUAGE];
} WEATHER_DESCRIPTIONS[] = {
	{  0, { "clear sky",            "jasno" } },
	{  1, { "mainly clear",         "skoro jasno" } },
	{  2, { "partly cloudy",        "polojasno" } },
	{  3, { "overcast",             "zataženo" } },
	{ 45, { "fog",                  "mlha" } },
	{ 48, { "freezing fog",         "mrznoucí mlha" } },
	{ 51, { "light drizzle",        "slabé mrholení" } },
	{ 53, { "drizzle",              "mrholení" } },
	{ 55, { "dense drizzle",        "silné mrholení" } },
	{ 56, { "freezing drizzle",     "mrznoucí mrholení" } },
	{ 57, { "freezing drizzle",     "mrznoucí mrholení" } },
	{ 61, { "light rain",           "slabý déšť" } },
	{ 63, { "rain",                 "déšť" } },
	{ 65, { "heavy rain",           "silný déšť" } },
	{ 66, { "freezing rain",        "mrznoucí déšť" } },
	{ 67, { "heavy freezing rain",  "silný mrznoucí déšť" } },
	{ 71, { "light snow",           "slabé sněžení" } },
	{ 73, { "snow",                 "sněžení" } },
	{ 75, { "heavy snow",           "silné sněžení" } },
	{ 77, { "snow grains",          "sněhová zrna" } },
	{ 80, { "light showers",        "slabé přeháňky" } },
	{ 81, { "showers",              "přeháňky" } },
	{ 82, { "violent showers",      "prudké přeháňky" } },
	{ 85, { "snow showers",         "sněhové přeháňky" } },
	{ 86, { "heavy snow showers",   "silné sněhové přeháňky" } },
	{ 95, { "thunderstorm",         "bouřka" } },
	{ 96, { "thunderstorm, hail",   "bouřka s kroupami" } },
	{ 99, { "thunderstorm, hail",   "bouřka s kroupami" } },
};

const char* weather_description(uint8_t code, LANGUAGE lang) {
	for (unsigned i = 0; i < sizeof(WEATHER_DESCRIPTIONS) / sizeof(WEATHER_DESCRIPTIONS[0]); i++) {
		if (WEATHER_DESCRIPTIONS[i].code == code) return WEATHER_DESCRIPTIONS[i].text[lang];
	}
	return "";
}
