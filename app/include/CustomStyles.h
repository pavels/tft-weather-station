#ifndef APP_CUSTOMSTYLES_H_
#define APP_CUSTOMSTYLES_H_

#include "lvgl.h"

/* Inner padding of the config screen's tab pages, px (top and sides). */
#define CONFIG_TAB_PAD 6

void custom_styles_init(void);

/* Black background, no padding, no border: for full-screen containers and the
 * config screen's tab chrome. */
lv_style_t* st_screen_bg(void);
/* Config screen tab page: st_screen_bg plus CONFIG_TAB_PAD on top and sides. */
lv_style_t* st_config_tab(void);
lv_style_t* st_transparent_bg(void);

/* Text: small = dejavu_14, medium = dejavu_30, large = dejavu_40. */
lv_style_t* st_small_white_text(void);
lv_style_t* st_small_yellow_text(void);
lv_style_t* st_small_blue_text(void);
lv_style_t* st_medium_white_text(void);
lv_style_t* st_medium_gray_text(void);
lv_style_t* st_medium_yellow_text(void);
lv_style_t* st_large_white_text(void);

lv_style_t* st_text_center(void);
lv_style_t* st_text_right(void);

/* Borderless, transparent button showing only its symbol label. */
lv_style_t* st_icon_button(void);
lv_style_t* st_icon_button_pressed(void);

#endif /* APP_CUSTOMSTYLES_H_ */
