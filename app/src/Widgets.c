#include "Widgets.h"
#include "CustomStyles.h"

lv_obj_t* create_screen_container(lv_obj_t* parent) {
	lv_obj_t* container = lv_obj_create(parent);
	lv_obj_set_size(container, lv_pct(100), lv_pct(100));
	lv_obj_add_style(container, st_screen_bg(), 0);
	lv_obj_set_scrollable(container, false);
	return container;
}

lv_obj_t* create_label(lv_obj_t* parent, const char* text, lv_style_t* text_style, lv_style_t* align_style,
		int32_t x, int32_t y, int32_t width) {
	lv_obj_t* label = lv_label_create(parent);
	lv_label_set_text(label, text);
	if(text_style) lv_obj_add_style(label, text_style, 0);
	if(align_style) lv_obj_add_style(label, align_style, 0);
	lv_obj_set_width(label, width);
	lv_obj_set_pos(label, x, y);
	return label;
}

lv_obj_t* create_settings_button(lv_obj_t* parent) {
	lv_obj_t* button = lv_button_create(parent);
	lv_obj_set_size(button, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_add_style(button, st_icon_button(), 0);
	lv_obj_add_style(button, st_icon_button_pressed(), LV_STATE_PRESSED);

	lv_obj_t* symbol = lv_label_create(button);
	lv_label_set_text(symbol, LV_SYMBOL_SETTINGS);
	return button;
}
