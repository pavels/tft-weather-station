#ifndef APP_CLOCK_H_
#define APP_CLOCK_H_

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Local time is the system clock (UTC, set by SNTP on the device, the host
 * clock in the simulator) plus the UTC offset of the configured city. All
 * times handed to the screens are "local time as UTC": format them with
 * gmtime(), not localtime(). */

/* Offset of the city's timezone from UTC in seconds, DST included. */
void clock_set_utc_offset(int32_t seconds);

/* False until the system clock has been set (first SNTP sync). */
bool clock_is_set(void);

time_t get_current_time(void);

#endif /* APP_CLOCK_H_ */
