#include <time.h>
#include <stdio.h>

#include "MainScreens.h"
#include "Controller.h"
#include "Lang.h"
#include "Widgets.h"
#include "WeatherIcons.h"

/* Forecast cells, three per row: 3 x 73px leaves ~10px screen margins, and
 * fits a 50px small weather icon plus padding. */
#define CELL_W 73
#define CELL_X(i) CENTERED_COL_X(i, 3, CELL_W)
/* Vertical pitch of the forecast screen's rows, px: a cell is 94px tall (day
 * label 15px above the temperature line, down to the end of the rain line),
 * plus an 8px gap. */
#define FORECAST_ROW_H 102

/* Indoor temperature | humidity: two columns spanning the same width as the
 * three forecast cells above them. */
#define HALF_COL_W (3 * CELL_W / 2)
#define HALF_COL_X(i) CENTERED_COL_X(i, 2, HALF_COL_W)

/* Forecast slots, in hours from today's midnight: today and tomorrow at
 * 8:00, 14:00 and 22:00. */
static const int32_t UPCOMING_SLOTS[] = { 8, 14, 22, 24 + 8, 24 + 14, 24 + 22 };
/* The three cells of a forecast screen row, in hours of that day. */
static const int32_t DAY_SLOTS[3] = { 8, 14, 22 };
/* A slot drops off the title screen this many hours before its time. */
#define SLOT_DROP_OFF_H 1

/* Yellow instead of gray while WiFi is down. */
#define STATE_WIFI_DOWN LV_STATE_USER_1

static lv_obj_t* wifi_status_field;

static lv_obj_t* date_field;
static lv_obj_t* time_field;

static lv_obj_t* local_temperature_field;
static lv_obj_t* local_humidity_field;

static void create_forecast_cell(lv_obj_t* container, const HourlyForecast* forecast, int32_t x, int32_t y) {
	char value_buf[32];
	struct tm *time_info = gmtime(&forecast->time);

	snprintf(value_buf, sizeof(value_buf), "%s %02d:%02d", WDAY_NAMES[weather_station_config.language][time_info->tm_wday], time_info->tm_hour, time_info->tm_min);
	create_label(container, value_buf, st_small_yellow_text(), st_text_center(), x, y - 15, CELL_W);

	snprintf(value_buf, sizeof(value_buf), "%.1f°C", forecast->temp);
	create_label(container, value_buf, st_small_white_text(), st_text_center(), x, y, CELL_W);

	lv_obj_t* icon = lv_image_create(container);
	lv_image_set_src(icon, weather_icon_small(forecast->weather_code));
	lv_obj_set_pos(icon, x + 11, y + 15);

	snprintf(value_buf, sizeof(value_buf), "%.1fmm", forecast->rain);
	create_label(container, value_buf, st_small_blue_text(), st_text_center(), x, y + 65, CELL_W);
}

static time_t today_midnight(void) {
	return get_current_time() / 86400 * 86400;
}

lv_obj_t* create_title_screen(lv_obj_t* screen, const City* city, const WeatherData* weather){
	char value_buf[32];
	lv_obj_t* main_container = create_screen_container(screen);

	lv_obj_t* btn_settings = create_settings_button(main_container);
	lv_obj_set_pos(btn_settings, 180, 5);
	lv_obj_add_event_cb(btn_settings, settings_click_action, LV_EVENT_CLICKED, NULL);

	wifi_status_field = create_label(main_container, LV_SYMBOL_WIFI, st_medium_gray_text(), NULL, 11, 14, LV_SIZE_CONTENT);
	lv_obj_add_style(wifi_status_field, st_medium_yellow_text(), STATE_WIFI_DOWN);
	update_wifi_status();

	/* Filled in by set_time(). */
	date_field = create_label(main_container, "", st_small_white_text(), st_text_center(), ROW_X, 15, ROW_W);
	time_field = create_label(main_container, "", st_large_white_text(), st_text_center(), ROW_X, 30, ROW_W);

	const CurrentWeather* current = &weather->current;
	lv_obj_t* weather_icon = lv_image_create(main_container);
	lv_image_set_src(weather_icon, weather_icon_big(current->valid ? current->weather_code : WEATHER_CODE_NONE));
	lv_obj_set_pos(weather_icon, 0, 65);

	create_label(main_container, city->valid ? city->name : "", st_small_blue_text(), st_text_right(), ROW_X, 75, ROW_W);
	if(current->valid) {
		snprintf(value_buf, sizeof(value_buf), "%.1f°C", current->temp);
		create_label(main_container, value_buf, st_large_white_text(), st_text_right(), ROW_X, 88, ROW_W);
		create_label(main_container, weather_description(current->weather_code, weather_station_config.language),
				st_small_yellow_text(), st_text_right(), ROW_X, 128, ROW_W);
	}

	/* The next three upcoming forecast slots. */
	time_t today = today_midnight();
	int time_of_day = get_current_time() - today;
	int first_slot = 0;
	while(first_slot < 3 && time_of_day >= (UPCOMING_SLOTS[first_slot] - SLOT_DROP_OFF_H) * 3600) first_slot++;
	for(int i = 0; i < 3; i++) {
		const HourlyForecast* forecast = weather_forecast_at(weather, today + UPCOMING_SLOTS[first_slot + i] * 3600);
		if(forecast) create_forecast_cell(main_container, forecast, CELL_X(i), 175);
	}

	create_label(main_container, lang_strings[STR_INDOOR_TEMP][weather_station_config.language], st_small_yellow_text(), st_text_center(),
			HALF_COL_X(0), 265, HALF_COL_W);
	create_label(main_container, lang_strings[STR_INDOOR_HUMIDITY][weather_station_config.language], st_small_yellow_text(), st_text_center(),
			HALF_COL_X(1), 265, HALF_COL_W);
	/* Filled in by set_local_cond(). */
	local_temperature_field = create_label(main_container, "", st_medium_white_text(), st_text_center(), HALF_COL_X(0), 280, HALF_COL_W);
	local_humidity_field = create_label(main_container, "", st_medium_white_text(), st_text_center(), HALF_COL_X(1), 280, HALF_COL_W);

	set_time();
	set_local_cond();

	return main_container;
}

lv_obj_t* create_forecast_screen(lv_obj_t* screen, const WeatherData* weather){
	lv_obj_t* main_container = create_screen_container(screen);

	time_t today = today_midnight();
	for(int day = 1; day <= 3; day++) {
		for(int i = 0; i < 3; i++) {
			const HourlyForecast* forecast = weather_forecast_at(weather, today + day * 86400 + DAY_SLOTS[i] * 3600);
			if(forecast) create_forecast_cell(main_container, forecast, CELL_X(i), 30 + (day - 1) * FORECAST_ROW_H);
		}
	}

	return main_container;
}

void destroy_title_screen(lv_obj_t* container){
	wifi_status_field = NULL;
	date_field = NULL;
	time_field = NULL;
	local_temperature_field = NULL;
	local_humidity_field = NULL;
	lv_obj_delete(container);
}

void destroy_forecast_screen(lv_obj_t* container){
	lv_obj_delete(container);
}

void update_wifi_status(void){
	if(wifi_status_field == NULL) return;
	/* No-op, so no redraw, while the state is unchanged. */
	lv_obj_set_state(wifi_status_field, STATE_WIFI_DOWN, !wifi_connected);
}

void set_time(void){
	time_t local_current_time = get_current_time();
	if(date_field == NULL || time_field == NULL) return;
	if(!clock_is_set()) {
		lv_label_set_text(date_field, "");
		lv_label_set_text(time_field, "--:--");
		return;
	}
	struct tm *time_info = gmtime(&local_current_time);
	char date_buf[32];
	char time_buf[8];

	snprintf(date_buf, sizeof(date_buf), "%s %s %i %i", WDAY_NAMES[weather_station_config.language][time_info->tm_wday], MONTH_NAMES[weather_station_config.language][time_info->tm_mon], time_info->tm_mday, 1900 + time_info->tm_year);
	snprintf(time_buf, sizeof(time_buf), "%02d:%02d", time_info->tm_hour, time_info->tm_min);

	lv_label_set_text(date_field, date_buf);
	lv_label_set_text(time_field, time_buf);
}

void set_local_cond(void){
	char value_buf[32];

	if(local_temperature_field == NULL || local_humidity_field == NULL) return;

	snprintf(value_buf,sizeof(value_buf),"%.1f°C", local_temperature);
	lv_label_set_text(local_temperature_field, value_buf);

	snprintf(value_buf,sizeof(value_buf),"%.0f%%", local_humidity);
	lv_label_set_text(local_humidity_field, value_buf);
}
