#include "CustomStyles.h"
#include "resources.h"

/* Palette, RGB hex. The accent yellow is also the theme's primary color
 * (active tab, checked buttons, slider); dark gray is the pressed icon button. */
#define COLOR_ACCENT_YELLOW 0xf3a51c
#define COLOR_BLUE          0x2884c5
#define COLOR_GRAY          0xbbbbbb
#define COLOR_DARK_GRAY     0x888888

static lv_style_t screen_bg;
static lv_style_t config_tab;
static lv_style_t transparent_bg;

static lv_style_t small_white_text;
static lv_style_t small_yellow_text;
static lv_style_t small_blue_text;
static lv_style_t medium_white_text;
static lv_style_t medium_gray_text;
static lv_style_t medium_yellow_text;
static lv_style_t large_white_text;

static lv_style_t text_center;
static lv_style_t text_right;

static lv_style_t icon_button;
static lv_style_t icon_button_pressed;

static void text_style_init(lv_style_t* style, lv_color_t color, const lv_font_t* font)
{
	lv_style_init(style);
	lv_style_set_text_color(style, color);
	lv_style_set_text_font(style, font);
}

void custom_styles_init(void)
{
	lv_theme_default_init(NULL, lv_color_hex(COLOR_ACCENT_YELLOW), lv_palette_main(LV_PALETTE_BLUE), true, &dejavu_20);

	lv_style_init(&screen_bg);
	lv_style_set_bg_color(&screen_bg, lv_color_black());
	lv_style_set_bg_opa(&screen_bg, LV_OPA_COVER);
	lv_style_set_pad_all(&screen_bg, 0);
	lv_style_set_border_width(&screen_bg, 0);

	lv_style_init(&config_tab);
	lv_style_set_bg_color(&config_tab, lv_color_black());
	lv_style_set_bg_opa(&config_tab, LV_OPA_COVER);
	lv_style_set_border_width(&config_tab, 0);
	lv_style_set_pad_hor(&config_tab, CONFIG_TAB_PAD);
	lv_style_set_pad_top(&config_tab, CONFIG_TAB_PAD);
	lv_style_set_pad_bottom(&config_tab, 0);

	lv_style_init(&transparent_bg);
	lv_style_set_bg_opa(&transparent_bg, LV_OPA_TRANSP);

	text_style_init(&small_white_text, lv_color_white(), &dejavu_14);
	text_style_init(&small_yellow_text, lv_color_hex(COLOR_ACCENT_YELLOW), &dejavu_14);
	text_style_init(&small_blue_text, lv_color_hex(COLOR_BLUE), &dejavu_14);
	text_style_init(&medium_white_text, lv_color_white(), &dejavu_30);
	text_style_init(&medium_gray_text, lv_color_hex(COLOR_GRAY), &dejavu_30);
	text_style_init(&medium_yellow_text, lv_color_hex(COLOR_ACCENT_YELLOW), &dejavu_30);
	text_style_init(&large_white_text, lv_color_white(), &dejavu_40);

	lv_style_init(&text_center);
	lv_style_set_text_align(&text_center, LV_TEXT_ALIGN_CENTER);

	lv_style_init(&text_right);
	lv_style_set_text_align(&text_right, LV_TEXT_ALIGN_RIGHT);

	text_style_init(&icon_button, lv_color_hex(COLOR_GRAY), &dejavu_30);
	lv_style_set_bg_opa(&icon_button, LV_OPA_TRANSP);
	lv_style_set_border_width(&icon_button, 0);

	lv_style_init(&icon_button_pressed);
	lv_style_set_text_color(&icon_button_pressed, lv_color_hex(COLOR_DARK_GRAY));
}

lv_style_t* st_screen_bg(void) { return &screen_bg; }
lv_style_t* st_config_tab(void) { return &config_tab; }
lv_style_t* st_transparent_bg(void) { return &transparent_bg; }

lv_style_t* st_small_white_text(void) { return &small_white_text; }
lv_style_t* st_small_yellow_text(void) { return &small_yellow_text; }
lv_style_t* st_small_blue_text(void) { return &small_blue_text; }
lv_style_t* st_medium_white_text(void) { return &medium_white_text; }
lv_style_t* st_medium_gray_text(void) { return &medium_gray_text; }
lv_style_t* st_medium_yellow_text(void) { return &medium_yellow_text; }
lv_style_t* st_large_white_text(void) { return &large_white_text; }

lv_style_t* st_text_center(void) { return &text_center; }
lv_style_t* st_text_right(void) { return &text_right; }

lv_style_t* st_icon_button(void) { return &icon_button; }
lv_style_t* st_icon_button_pressed(void) { return &icon_button_pressed; }
