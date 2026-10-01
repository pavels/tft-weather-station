#include <stdio.h>
#include <string.h>

#include "ConfigScreen.h"
#include "Controller.h"
#include "Config.h"
#include "Widgets.h"

#include "../version.h"

#if(TARGET_ESP8266 == 1)
#include "esp_system.h"
#endif

/* Roller option lists: the selected index is the hour/minute value itself. */
static const char *HOUR_ROLLER_OPTIONS =
	"0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23";
static const char *MINUTE_ROLLER_OPTIONS =
	"00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59";

/* All sizes are fixed px, not derived from LV_DPI_DEF. */

/* Vertical rhythm of the tab stacks, px: caption -> its field, field -> next caption. */
#define LABEL_GAP 4
#define SECTION_GAP 10
/* Sliders sit inset from the content edges: the knob overhangs both ends of
 * the bar (more so while pressed), and must stay on screen. */
#define SLIDER_INSET 12
/* One roller, px; two side by side make an hour:minute pair. */
#define ROLLER_WIDTH 50
#define ROLLER_VISIBLE_ROWS 3
/* The theme's DPI-scaled row spacing (~15px) makes 3 roller rows too tall for
 * the Power tab; 8px gives ~30px rows with dejavu_20. */
#define ROLLER_LINE_SPACE 8

/* Tab bar height, px. The default, LV_DPI_DEF / 2 (65px), leaves the tab
 * content too little room. */
#define TAB_BAR_H 36
/* Space left below the tabview for the "Save settings" button, px. */
#define SAVE_BUTTON_AREA_H 40
/* On-screen keyboard height, px: covers the lower part of the screen, leaving
 * the edited field visible above it. */
#define KEYBOARD_H 140

/* Slider bar height, px. */
#define SLIDER_H 20
/* Extra gap above a slider, px: its knob sticks out above the bar. */
#define SLIDER_KNOB_OVERHANG 4
/* Backlight brightness range, as ili9341_set_backlight() takes it. The "on"
 * brightness can't go below ON_BRIGHTNESS_MIN, so the screen can't be set
 * unreadably dark during the day. */
#define BRIGHTNESS_MAX 255
#define ON_BRIGHTNESS_MIN 64

/* Language selector height and its inner padding/gap, px. The theme's
 * DPI-scaled padding leaves the buttons ~18px tall; this lets them fill the
 * control so they're easy targets. */
#define LANGUAGE_SELECTOR_H 48

/* Space under the city id field for the resolved place: two lines of dejavu_14
 * (a long name/region/country wraps), px. */
#define CITY_PLACE_H 36
#define LANGUAGE_SELECTOR_PAD 4

static lv_obj_t *config_kb;
static lv_obj_t *config_container;

static lv_obj_t *wifi_ssid_field;
static lv_obj_t *password_field;

static lv_obj_t *cityid_field;
static lv_obj_t *city_place_label;

static lv_obj_t *roller_power_hour_off;
static lv_obj_t *roller_power_min_off;

static lv_obj_t *roller_power_hour_on;
static lv_obj_t *roller_power_min_on;

static lv_obj_t *slider_brightness_off;
static lv_obj_t *slider_brightness_on;
static lv_obj_t *language_selector;

static void config_keyboard_open_cb(lv_event_t *e);
static void config_keyboard_close_cb(lv_event_t *e);
static void config_tab_changed_cb(lv_event_t *e);
static void city_field_ready_cb(lv_event_t *e);
static void check_update_cb(lv_event_t *e);
static void save_config_screen_action(lv_event_t *e);

/* Deleted asynchronously: this runs inside the keyboard's own READY/CANCEL
 * event, and deleting it there would stop LVGL from also sending READY to the
 * text area (the city id lookup listens for that). */
static void config_keyboard_hide(void) {
	if(config_kb == NULL) return;
	lv_obj_delete_async(config_kb);
	config_kb = NULL;
}

/* Caption at the top of the tab, or SECTION_GAP below `below`. */
static lv_obj_t* create_caption(lv_obj_t* tab, const char* text, lv_obj_t* below) {
	lv_obj_t* label = lv_label_create(tab);
	lv_label_set_text(label, text);
	lv_obj_add_style(label, st_small_white_text(), 0);
	if(below) lv_obj_align_to(label, below, LV_ALIGN_OUT_BOTTOM_LEFT, 0, SECTION_GAP);
	return label;
}

static lv_obj_t* create_text_field(lv_obj_t* tab, lv_obj_t* caption, const char* text) {
	lv_obj_t* field = lv_textarea_create(tab);
	lv_textarea_set_one_line(field, true);
	lv_obj_set_width(field, lv_pct(100));
	lv_obj_align_to(field, caption, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LABEL_GAP);
	lv_textarea_set_text(field, text);
	lv_obj_add_event_cb(field, config_keyboard_open_cb, LV_EVENT_FOCUSED, NULL);
	return field;
}

static lv_obj_t* create_roller_pair_title(lv_obj_t* tab, const char* text, lv_align_t align) {
	lv_obj_t* label = lv_label_create(tab);
	lv_label_set_text(label, text);
	lv_obj_add_style(label, st_small_white_text(), 0);
	lv_obj_add_style(label, st_text_center(), 0);
	lv_obj_set_width(label, 2 * ROLLER_WIDTH);
	lv_obj_align(label, align, 0, 0);
	return label;
}

static lv_obj_t* create_time_roller(lv_obj_t* tab, const char* options, uint32_t selected) {
	lv_obj_t* roller = lv_roller_create(tab);
	lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
	lv_obj_set_width(roller, ROLLER_WIDTH);
	lv_obj_set_style_text_line_space(roller, ROLLER_LINE_SPACE, 0);
	lv_roller_set_visible_row_count(roller, ROLLER_VISIBLE_ROWS);
	lv_roller_set_selected(roller, selected, LV_ANIM_OFF);
	return roller;
}

static lv_obj_t* create_brightness_slider(lv_obj_t* tab, lv_obj_t* caption, int32_t min, int32_t value) {
	lv_obj_t* slider = lv_slider_create(tab);
	lv_obj_set_size(slider, LV_HOR_RES - 2 * CONFIG_TAB_PAD - 2 * SLIDER_INSET, SLIDER_H);
	lv_obj_align_to(slider, caption, LV_ALIGN_OUT_BOTTOM_LEFT, SLIDER_INSET, LABEL_GAP + SLIDER_KNOB_OVERHANG);
	lv_slider_set_range(slider, min, BRIGHTNESS_MAX);
	lv_slider_set_value(slider, value, LV_ANIM_OFF);
	return slider;
}

static lv_obj_t* create_tabview(lv_obj_t* parent) {
	lv_obj_t *tabview = lv_tabview_create(parent);
	lv_obj_set_size(tabview, lv_pct(100), LV_VER_RES - SAVE_BUTTON_AREA_H);
	lv_obj_add_style(tabview, st_screen_bg(), 0);
	lv_obj_add_event_cb(tabview, config_tab_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
	lv_tabview_set_tab_bar_size(tabview, TAB_BAR_H);
	lv_obj_add_style(lv_tabview_get_tab_bar(tabview), st_screen_bg(), 0);
	return tabview;
}

static lv_obj_t* add_tab(lv_obj_t* tabview, const char* name) {
	lv_obj_t* tab = lv_tabview_add_tab(tabview, name);
	lv_obj_add_style(tab, st_config_tab(), 0);
	lv_obj_set_scrollable(tab, false);

	/* Transparent tab button, so only the active tab's indicator stands out.
	 * One selector per state: a style's state bits must all be present on the
	 * object to apply, so LV_STATE_ANY would never match. */
	lv_obj_t* tab_bar = lv_tabview_get_tab_bar(tabview);
	lv_obj_t* tab_btn = lv_obj_get_child(tab_bar, lv_obj_get_child_count(tab_bar) - 1);
	lv_obj_add_style(tab_btn, st_transparent_bg(), LV_STATE_DEFAULT);
	lv_obj_add_style(tab_btn, st_transparent_bg(), LV_STATE_CHECKED);
	return tab;
}

static void create_wifi_tab(lv_obj_t* tab) {
	lv_obj_t* network_caption = create_caption(tab, "Network", NULL);
	wifi_ssid_field = create_text_field(tab, network_caption, weather_station_config.wifi_ssid);

	lv_obj_t* password_caption = create_caption(tab, "Password", wifi_ssid_field);
	password_field = create_text_field(tab, password_caption, weather_station_config.wifi_password);
	lv_textarea_set_password_mode(password_field, true);
}

/* "Praha, Praha, Česko": what the city id resolves to. */
static void show_city_place(const City* city) {
	char place[sizeof(city->name) + sizeof(city->region) + sizeof(city->country) + 4];
	snprintf(place, sizeof(place), "%s, %s, %s", city->name, city->region, city->country);
	lv_label_set_text(city_place_label, place);
}

static void create_city_tab(lv_obj_t* tab) {
	lv_obj_t* city_caption = create_caption(tab, "City ID (GeoNames)", NULL);
	cityid_field = create_text_field(tab, city_caption, weather_station_config.city_id);
	lv_obj_add_event_cb(cityid_field, city_field_ready_cb, LV_EVENT_READY, NULL);

	city_place_label = lv_label_create(tab);
	lv_obj_add_style(city_place_label, st_small_yellow_text(), 0);
	lv_obj_set_width(city_place_label, lv_pct(100));
	lv_obj_align_to(city_place_label, cityid_field, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LABEL_GAP);
	if (current_city.valid && strcmp(current_city.id, weather_station_config.city_id) == 0) {
		show_city_place(&current_city);
	} else {
		lv_label_set_text(city_place_label, "");
	}

	/* Weather API language, weekday/month names and UI strings. One button
	 * per LANGUAGE, exactly one checked. Placed for a two-line place label. */
	lv_obj_t* language_caption = create_caption(tab, "Language", NULL);
	lv_obj_align_to(language_caption, cityid_field, LV_ALIGN_OUT_BOTTOM_LEFT, 0, CITY_PLACE_H + SECTION_GAP);

	static const char *language_map[LAST_LANGUAGE + 1];
	for (int i = 0; i < LAST_LANGUAGE; i++) language_map[i] = LANGUAGE_NAMES[i];
	language_map[LAST_LANGUAGE] = "";

	language_selector = lv_buttonmatrix_create(tab);
	lv_buttonmatrix_set_map(language_selector, language_map);
	lv_buttonmatrix_set_button_ctrl_all(language_selector, LV_BUTTONMATRIX_CTRL_CHECKABLE);
	lv_buttonmatrix_set_one_checked(language_selector, true);
	lv_buttonmatrix_set_button_ctrl(language_selector, weather_station_config.language, LV_BUTTONMATRIX_CTRL_CHECKED);
	lv_obj_set_size(language_selector, lv_pct(100), LANGUAGE_SELECTOR_H);
	lv_obj_set_style_pad_all(language_selector, LANGUAGE_SELECTOR_PAD, 0);
	lv_obj_set_style_pad_gap(language_selector, LANGUAGE_SELECTOR_PAD, 0);
	lv_obj_align_to(language_selector, language_caption, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LABEL_GAP);
}

static void create_power_tab(lv_obj_t* tab) {
	lv_obj_t* off_title = create_roller_pair_title(tab, "Power OFF", LV_ALIGN_TOP_LEFT);
	lv_obj_t* on_title = create_roller_pair_title(tab, "Power ON", LV_ALIGN_TOP_RIGHT);

	roller_power_hour_off = create_time_roller(tab, HOUR_ROLLER_OPTIONS, weather_station_config.power_off_hour);
	lv_obj_align_to(roller_power_hour_off, off_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LABEL_GAP);
	roller_power_min_off = create_time_roller(tab, MINUTE_ROLLER_OPTIONS, weather_station_config.power_off_minute);
	lv_obj_align_to(roller_power_min_off, roller_power_hour_off, LV_ALIGN_OUT_RIGHT_TOP, 0, 0);

	roller_power_hour_on = create_time_roller(tab, HOUR_ROLLER_OPTIONS, weather_station_config.power_on_hour);
	lv_obj_align_to(roller_power_hour_on, on_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LABEL_GAP);
	roller_power_min_on = create_time_roller(tab, MINUTE_ROLLER_OPTIONS, weather_station_config.power_on_minute);
	lv_obj_align_to(roller_power_min_on, roller_power_hour_on, LV_ALIGN_OUT_RIGHT_TOP, 0, 0);

	lv_obj_t* off_brightness_caption = create_caption(tab, "Brightness when OFF", roller_power_hour_off);
	slider_brightness_off = create_brightness_slider(tab, off_brightness_caption, 0, weather_station_config.power_off_brightness);

	/* Back at the content edge, undoing the slider's inset. */
	lv_obj_t* on_brightness_caption = create_caption(tab, "Brightness when ON", NULL);
	lv_obj_align_to(on_brightness_caption, slider_brightness_off, LV_ALIGN_OUT_BOTTOM_LEFT, -SLIDER_INSET, SECTION_GAP);
	slider_brightness_on = create_brightness_slider(tab, on_brightness_caption, ON_BRIGHTNESS_MIN, weather_station_config.power_on_brightness);
}

static void create_info_tab(lv_obj_t* tab) {
	char version_buf[32];
	snprintf(version_buf, sizeof(version_buf), "Version: %s", OTAVERSION);
	lv_obj_t* version_caption = create_caption(tab, version_buf, NULL);

	/* Open-Meteo's data is CC BY 4.0, which asks for attribution. */
	lv_obj_t* attribution = create_caption(tab, "Weather data: Open-Meteo.com", version_caption);
	lv_obj_t* license = create_caption(tab, "(CC BY 4.0)", attribution);

	lv_obj_t* btn_update = lv_button_create(tab);
	lv_obj_set_size(btn_update, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_align_to(btn_update, license, LV_ALIGN_OUT_BOTTOM_LEFT, 0, SECTION_GAP);
	lv_obj_add_event_cb(btn_update, check_update_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_t* lbl_update = lv_label_create(btn_update);
	lv_label_set_text(lbl_update, "Check for updates");
}

lv_obj_t* create_config_screen(lv_obj_t* screen) {
	config_container = create_screen_container(screen);

	lv_obj_t *tabview = create_tabview(config_container);
	create_wifi_tab(add_tab(tabview, "WiFi"));
	create_city_tab(add_tab(tabview, "City"));
	create_power_tab(add_tab(tabview, "Power"));
	create_info_tab(add_tab(tabview, "Info"));

	lv_obj_t* btn_save = lv_button_create(config_container);
	lv_obj_set_size(btn_save, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_align(btn_save, LV_ALIGN_BOTTOM_MID, 0, 0);
	lv_obj_add_event_cb(btn_save, save_config_screen_action, LV_EVENT_CLICKED, NULL);

	lv_obj_t * lbl_btn_save = lv_label_create(btn_save);
	lv_label_set_text(lbl_btn_save, "Save settings");

	return config_container;
}

static void config_keyboard_open_cb(lv_event_t *e) {
	lv_obj_t *text_area = lv_event_get_target_obj(e);

	if(config_kb) {
		lv_keyboard_set_textarea(config_kb, text_area);
	} else {
		config_kb = lv_keyboard_create(config_container);
		/* lv_keyboard_create() already aligns it to the bottom edge. */
		lv_obj_set_height(config_kb, KEYBOARD_H);
		lv_keyboard_set_textarea(config_kb, text_area);
		lv_obj_add_event_cb(config_kb, config_keyboard_close_cb, LV_EVENT_READY, NULL);
		lv_obj_add_event_cb(config_kb, config_keyboard_close_cb, LV_EVENT_CANCEL, NULL);
	}
}

static void config_keyboard_close_cb(lv_event_t *e) {
	(void)e;
	config_keyboard_hide();
}

static void config_tab_changed_cb(lv_event_t *e) {
	(void)e;
	config_keyboard_hide();
}

static void check_update_cb(lv_event_t *e) {
	(void)e;
	platform_manual_update();
}

/* Keyboard OK on the city id: look it up right away, so the place shows
 * before saving. */
static void city_field_ready_cb(lv_event_t *e) {
	(void)e;
	static City city;
	const char* id = lv_textarea_get_text(cityid_field);

	if (id[0] == '\0') {
		lv_label_set_text(city_place_label, "");
	} else if (city_resolve(id, weather_station_config.language, &city)) {
		show_city_place(&city);
	} else {
		lv_label_set_text(city_place_label, "City not found");
	}
}

static void save_config_screen_action(lv_event_t *e) {
	(void)e;

	strncpy(weather_station_config.wifi_ssid, lv_textarea_get_text(wifi_ssid_field), sizeof(weather_station_config.wifi_ssid) - 1);
	strncpy(weather_station_config.wifi_password, lv_textarea_get_text(password_field), sizeof(weather_station_config.wifi_password) - 1);
	strncpy(weather_station_config.city_id, lv_textarea_get_text(cityid_field), sizeof(weather_station_config.city_id) - 1);

	weather_station_config.power_off_hour = lv_roller_get_selected(roller_power_hour_off);
	weather_station_config.power_off_minute = lv_roller_get_selected(roller_power_min_off);

	weather_station_config.power_on_hour = lv_roller_get_selected(roller_power_hour_on);
	weather_station_config.power_on_minute = lv_roller_get_selected(roller_power_min_on);

	for (int i = 0; i < LAST_LANGUAGE; i++) {
		if (lv_buttonmatrix_has_button_ctrl(language_selector, i, LV_BUTTONMATRIX_CTRL_CHECKED)) {
			weather_station_config.language = (LANGUAGE)i;
		}
	}

	weather_station_config.power_off_brightness = lv_slider_get_value(slider_brightness_off);
	weather_station_config.power_on_brightness = lv_slider_get_value(slider_brightness_on);

	store_ws_config(&weather_station_config);

	#if(TARGET_ESP8266 == 1)
		esp_restart();
	#else
		lv_obj_delete(config_container);
		load_remote_data();
		display_title_screen();
	#endif
}
