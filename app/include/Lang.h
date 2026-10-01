#ifndef APP_LANG_H_
#define APP_LANG_H_

#include <stdint.h>

typedef enum { ENGLISH, CZECH, LAST_LANGUAGE } LANGUAGE;

typedef enum { STR_LANG_CODE, STR_CONNECTING, STR_INDOOR_TEMP, STR_INDOOR_HUMIDITY, LAST_STR } STR;

/* Each language's own name, for the config screen's language selector. */
extern const char* LANGUAGE_NAMES[LAST_LANGUAGE];
extern const char* WDAY_NAMES[LAST_LANGUAGE][7];
extern const char* MONTH_NAMES[LAST_LANGUAGE][12];
extern const char* lang_strings[LAST_STR][LAST_LANGUAGE];

/* Short description of a WMO weather code (Open-Meteo's weather_code). */
const char* weather_description(uint8_t code, LANGUAGE lang);

#endif /* APP_LANG_H_ */
