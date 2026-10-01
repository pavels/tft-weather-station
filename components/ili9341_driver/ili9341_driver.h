#ifndef ILI9341_DRIVER_H_
#define ILI9341_DRIVER_H_

#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Panel resolution, px, in its mounted portrait orientation. */
#define ILI9341_HOR_RES 240
#define ILI9341_VER_RES 320

/* Board wiring. The backlight pin is PWM-dimmed (ili9341_set_backlight). */
#define ILI9341_CS_GPIO   5
#define ILI9341_DC_GPIO   4
#define ILI9341_BCKL_GPIO 15

/*
 * Sends the LCD init command sequence and starts the backlight PWM.
 * Assumes the shared HSPI bus is already up (spi_bus_init()) -- it's shared
 * with the touch controller (xpt2046_driver).
 */
void ili9341_init(void);

/* brightness: 0 (off) - 255 (full), gamma-corrected. */
void ili9341_set_backlight(uint8_t brightness);

/* Direct drawing, for when LVGL isn't running (update mode). Colors are
 * RGB565; coordinates in the portrait panel's pixels. */
void ili9341_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color);

/* Starts a w x h pixel window at (x, y); fill it row by row with
 * ili9341_write_pixels(), in the panel's byte order (RGB565, high byte first). */
void ili9341_begin_pixels(int32_t x, int32_t y, int32_t w, int32_t h);
void ili9341_write_pixels(const uint8_t *data, size_t len);

/* LVGL display flush callback -- register with lv_display_set_flush_cb().
 * Expects the display's color format set to LV_COLOR_FORMAT_RGB565_SWAPPED
 * so px_map is already in the panel's byte order. */
void ili9341_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

#ifdef __cplusplus
}
#endif

#endif /* ILI9341_DRIVER_H_ */
