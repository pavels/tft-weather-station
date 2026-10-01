#ifndef DHT_DRIVER_H_
#define DHT_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GPIO16 has no internal pull-up: the DHT22 module's own pull-up resistor is
 * required. */
#define DHT22_GPIO 16

typedef enum {
    DHT_OK = 0,
    DHT_ERR_PHASE_B,   /* sensor didn't pull the line low after the start pulse */
    DHT_ERR_PHASE_C,   /* ...didn't release it after its ~80us low */
    DHT_ERR_PHASE_D,   /* ...didn't start the first bit after its ~80us high */
    DHT_ERR_BIT_LOW,   /* a bit's low part timed out (bit index in *failed_bit) */
    DHT_ERR_BIT_HIGH,  /* a bit's high part timed out */
    DHT_ERR_CHECKSUM,
} dht_result_t;

/* Configures the pin: input, no internal pull-down (GPIO16's pad has one,
 * and it would fight the module's pull-up). Call once before reading. */
void dht22_init(void);

/*
 * One DHT22 (AM2302) reading, in tenths: 215 = 21.5 degC / 21.5 %RH.
 * Blocks ~6-7ms, ~5ms of it with interrupts off. On failure the caller
 * just retries later; the result says where the exchange broke down
 * (failed_bit may be NULL). The sensor needs >= 2s between reads, and
 * ~1-2s after power-up before the first.
 */
dht_result_t dht22_read(int16_t *humidity_x10, int16_t *temperature_x10, int *failed_bit);

/* Raw capture of the last read, for diagnosing decode problems: the five
 * bytes as decoded, and each bit's measured low/high time in microseconds
 * (a bit decodes as 1 when its high is over 48us). */
typedef struct {
    uint8_t data[5];
    uint8_t low[40];
    uint8_t high[40];
} dht_capture_t;

const dht_capture_t *dht22_last_capture(void);

const char *dht22_result_name(dht_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* DHT_DRIVER_H_ */
