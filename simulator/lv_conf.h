/*
 * Simulator-only LVGL config override. Everything platform-agnostic lives in
 * the single shared ../lvgl/lv_conf.h; this file only flips on the one thing
 * that's genuinely per-platform -- the SDL display/input drivers, which
 * can't exist on the ESP8266 side and would break that build if enabled
 * there.
 */
#ifndef SIMULATOR_LV_CONF_H_
#define SIMULATOR_LV_CONF_H_

#include "../lvgl/lv_conf.h"

#undef LV_USE_SDL
#define LV_USE_SDL 1

#endif /* SIMULATOR_LV_CONF_H_ */
