#ifndef XPT2046_DRIVER_H_
#define XPT2046_DRIVER_H_

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Both are boot-strapping pins, which must be high at reset: CS idles high,
 * and PENIRQ has a pull-up (so don't touch the screen while it boots). */
#define XPT2046_CS_GPIO  0
#define XPT2046_IRQ_GPIO 2

/* Logs raw ADC and calibrated coordinates for every press -- for checking
 * the calibration constants on real hardware. */
#ifndef XPT2046_DEBUG_LOG
#define XPT2046_DEBUG_LOG 0
#endif

/* Assumes the shared HSPI bus is already up (spi_bus_init()). */
void xpt2046_init(void);

/* LVGL pointer read callback -- register with lv_indev_set_read_cb().
 * Runs in the LVGL task, so it never overlaps a display flush on the bus. */
void xpt2046_read_cb(lv_indev_t *indev, lv_indev_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* XPT2046_DRIVER_H_ */
