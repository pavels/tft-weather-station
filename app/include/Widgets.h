#ifndef APP_WIDGETS_H_
#define APP_WIDGETS_H_

#include "lvgl.h"

/* Screen width in px (portrait panel). */
#define SCREEN_W 240

/* x of column `i` of `count` equal columns of width `w`, centered on the screen. */
#define CENTERED_COL_X(i, count, w) ((SCREEN_W - (count) * (w)) / 2 + (i) * (w))

/* Full-width text row: 200px, leaving 20px margins on each side. */
#define ROW_W 200
#define ROW_X CENTERED_COL_X(0, 1, ROW_W)

lv_obj_t* create_screen_container(lv_obj_t* parent);

/* Label at (x, y) with the given width (or LV_SIZE_CONTENT). `text_style` and
 * `align_style` (st_text_center / st_text_right) may be NULL. */
lv_obj_t* create_label(lv_obj_t* parent, const char* text, lv_style_t* text_style, lv_style_t* align_style,
		int32_t x, int32_t y, int32_t width);

/* Gray gear icon button; the caller positions it and attaches the click action. */
lv_obj_t* create_settings_button(lv_obj_t* parent);

#endif /* APP_WIDGETS_H_ */
