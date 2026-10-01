#ifndef APP_MAINSCREENS_H_
#define APP_MAINSCREENS_H_

#include "lvgl.h"
#include "CustomStyles.h"
#include "City.h"
#include "Weather.h"

lv_obj_t* create_title_screen(lv_obj_t* screen, const City* city, const WeatherData* weather);
lv_obj_t* create_forecast_screen(lv_obj_t* screen, const WeatherData* weather);
void destroy_title_screen(lv_obj_t* container);
void destroy_forecast_screen(lv_obj_t* container);

/* Title screen refreshers; no-ops while it isn't shown. */
void update_wifi_status(void);
void set_time(void);
void set_local_cond(void);

#endif /* APP_MAINSCREENS_H_ */
