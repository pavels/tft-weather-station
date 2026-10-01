#ifndef RESOURCES_RESOURCES_H_
#define RESOURCES_RESOURCES_H_

#include "lvgl.h"

extern const lv_font_t dejavu_14;
extern const lv_font_t dejavu_20;
extern const lv_font_t dejavu_30;
extern const lv_font_t dejavu_40;

/* Weather icons are not compiled in: the C arrays under resources/icons/ are
 * packed into icons.bin by gen_icon_blob.py and served via the "I:" drive
 * (IconFs.c). */

#endif /* RESOURCES_RESOURCES_H_ */
