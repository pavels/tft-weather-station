#include "Clock.h"

/* A system clock below this (2001-09-09) hasn't been set yet: it starts at 0
 * (1970) on boot. */
#define MIN_VALID_TIME 1000000000L

static int32_t utc_offset;

void clock_set_utc_offset(int32_t seconds) {
	utc_offset = seconds;
}

bool clock_is_set(void) {
	return time(NULL) > MIN_VALID_TIME;
}

time_t get_current_time(void) {
	return time(NULL) + utc_offset;
}
