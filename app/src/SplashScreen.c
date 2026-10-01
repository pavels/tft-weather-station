#include "SplashScreen.h"
#include "Controller.h"
#include "Lang.h"
#include "Widgets.h"

lv_obj_t* create_splash_screen(lv_obj_t* screen){
	lv_obj_t* main_container = create_screen_container(screen);

	create_label(main_container, LV_SYMBOL_WIFI, st_large_white_text(), st_text_center(), ROW_X, 50, ROW_W);
	create_label(main_container, lang_strings[STR_CONNECTING][weather_station_config.language], NULL, st_text_center(),
			ROW_X, 100, ROW_W);

	lv_obj_t* spinner = lv_spinner_create(main_container);
	lv_obj_set_size(spinner, 50, 50);
	lv_obj_set_pos(spinner, 95, 140);

	lv_obj_t* btn_settings = create_settings_button(main_container);
	lv_obj_align(btn_settings, LV_ALIGN_TOP_MID, 0, 200);
	lv_obj_add_event_cb(btn_settings, settings_click_action, LV_EVENT_CLICKED, NULL);

	return main_container;
}
