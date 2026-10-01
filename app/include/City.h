#ifndef APP_CITY_H_
#define APP_CITY_H_

#include <stdbool.h>

#include "Lang.h"

typedef struct {
	bool valid;
	char id[16];      /* GeoNames id, as stored in the config */
	char name[32];    /* in the requested language, e.g. "Praha" */
	char region[32];  /* first-level administrative area, e.g. "Středočeský kraj" */
	char country[24];
	float latitude;
	float longitude;
} City;

/* Looks a city up by its GeoNames id (Open-Meteo geocoding). Fills `city`
 * only on success; otherwise it is left as it was. */
bool city_resolve(const char* id, LANGUAGE lang, City* city);

#endif /* APP_CITY_H_ */
