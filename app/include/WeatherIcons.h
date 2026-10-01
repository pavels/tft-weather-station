#ifndef APP_WEATHERICONS_H_
#define APP_WEATHERICONS_H_

#include <stdint.h>

/* Not a WMO code: shows the "unknown" icon. */
#define WEATHER_CODE_NONE 255

/* Icon for a WMO weather code, as an LVGL image source ("I:..." path). */
const char* weather_icon_big(uint8_t code);
const char* weather_icon_small(uint8_t code);

#endif /* APP_WEATHERICONS_H_ */
