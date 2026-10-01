#include "dht_driver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/soc.h"
#include "esp_clk.h"
#include "rom/ets_sys.h"

/*
 * DHT22 driver, based on esp-open-rtos's dht.c (BSD, Jonathan Hartsuiker).
 *
 *  __           ______          _______                              ___________________________
 *    \    A    /      \   C    /       \   DHT duration_data_low    /                           \
 *     \_______/   B    \______/    D    \__________________________/   DHT duration_data_high    \__
 *
 *  A: host pulls low (start); B: host releases, sensor pulls low within
 *  20-40us; C: sensor low ~80us; D: sensor high ~80us. Then 40 bits, each a
 *  ~50us low followed by a high of ~26-28us for a 0 or ~70us for a 1. Bytes:
 *  humidity hi/lo, temperature hi/lo (bit 15 = sign), checksum = low byte of
 *  the sum of the other four.
 *
 * Bits are decoded against a fixed threshold in real microseconds (CPU cycle
 * counter). Comparing a bit's high against its own low is unreliable: the
 * sensor stretches the low before each byte to ~66us, about as long as a 1.
 *
 * Only the timed part (B onwards, ~5ms) runs with interrupts off. The start
 * pulse is a ~1ms busy-wait with interrupts on but the scheduler suspended, so
 * no task can stretch it and interrupts only by microseconds. (A task delay is
 * too coarse: 1 tick at 100Hz can be ~0ms, 2 ticks up to 20ms plus scheduling
 * latency.)
 */

/* 5 bytes: humidity hi/lo, temperature hi/lo, checksum. */
#define DHT_DATA_BITS 40
#define DHT_ONE_THRESHOLD_US 48 /* midway between a 0's ~27us and a 1's ~70us high */
#define DHT_SETTLE_US 2         /* skip edge jitter / our own release before polling */

/* Start pulse length, us: the DHT22's typical 1ms plus margin (spec 0.8-20ms). */
#define DHT_START_PULSE_US 1100

/* Timeouts per phase (see the diagram above), us: the spec's typical duration
 * plus margin. */
#define DHT_TIMEOUT_B_US        60  /* sensor answers within 20-40us */
#define DHT_TIMEOUT_C_US        120 /* sensor low ~80us */
#define DHT_TIMEOUT_D_US        120 /* sensor high ~80us */
#define DHT_TIMEOUT_BIT_LOW_US  100 /* ~50us, stretched to ~66us before each byte */
#define DHT_TIMEOUT_BIT_HIGH_US 110 /* ~27us for a 0, ~70us for a 1 */

static uint32_t ticks_per_us;

static dht_capture_t capture;

/* Waits up to `timeout_us` for the pin to reach `state`; the time it took
 * (from the call) goes to *duration_us. Runs with interrupts off, so the
 * tick ISR that resets the cycle counter can't run in between. */
static bool await_pin_state(uint32_t timeout_us, int state, uint32_t *duration_us)
{
    uint32_t start = soc_get_ccount();
    uint32_t limit = timeout_us * ticks_per_us;

    ets_delay_us(DHT_SETTLE_US);
    while (1) {
        uint32_t elapsed = soc_get_ccount() - start;
        if (gpio_get_level(DHT22_GPIO) == state) {
            if (duration_us) *duration_us = elapsed / ticks_per_us;
            return true;
        }
        if (elapsed > limit) return false;
    }
}

/* Everything after the start pulse; call with interrupts off. */
static dht_result_t fetch_bits(bool bits[DHT_DATA_BITS], int *failed_bit)
{
    uint32_t low_duration, high_duration;

    gpio_set_level(DHT22_GPIO, 1);
    gpio_set_direction(DHT22_GPIO, GPIO_MODE_INPUT);

    if (!await_pin_state(DHT_TIMEOUT_B_US, 0, NULL)) return DHT_ERR_PHASE_B;
    if (!await_pin_state(DHT_TIMEOUT_C_US, 1, NULL)) return DHT_ERR_PHASE_C;
    if (!await_pin_state(DHT_TIMEOUT_D_US, 0, NULL)) return DHT_ERR_PHASE_D;

    for (int i = 0; i < DHT_DATA_BITS; i++) {
        *failed_bit = i;
        if (!await_pin_state(DHT_TIMEOUT_BIT_LOW_US, 1, &low_duration)) return DHT_ERR_BIT_LOW;
        if (!await_pin_state(DHT_TIMEOUT_BIT_HIGH_US, 0, &high_duration)) return DHT_ERR_BIT_HIGH;
        bits[i] = high_duration > DHT_ONE_THRESHOLD_US;
        capture.low[i] = low_duration > 255 ? 255 : low_duration;
        capture.high[i] = high_duration > 255 ? 255 : high_duration;
    }
    return DHT_OK;
}

static int16_t convert(uint8_t msb, uint8_t lsb)
{
    int16_t value = ((msb & 0x7F) << 8) | lsb;
    return (msb & 0x80) ? -value : value;
}

void dht22_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << DHT22_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,     /* GPIO16 has none anyway */
        .pull_down_en = GPIO_PULLDOWN_DISABLE, /* would fight the module's pull-up */
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

const char *dht22_result_name(dht_result_t result)
{
    switch (result) {
    case DHT_OK: return "ok";
    case DHT_ERR_PHASE_B: return "no response to start pulse (phase B)";
    case DHT_ERR_PHASE_C: return "response low too long (phase C)";
    case DHT_ERR_PHASE_D: return "response high too long (phase D)";
    case DHT_ERR_BIT_LOW: return "bit low timeout";
    case DHT_ERR_BIT_HIGH: return "bit high timeout";
    case DHT_ERR_CHECKSUM: return "checksum mismatch";
    }
    return "?";
}

dht_result_t dht22_read(int16_t *humidity_x10, int16_t *temperature_x10, int *failed_bit)
{
    bool bits[DHT_DATA_BITS];
    uint8_t data[DHT_DATA_BITS / 8] = {0};
    int bit = -1;

    ticks_per_us = esp_clk_cpu_freq() / 1000000;

    /* Phase A: start pulse. The scheduler stays suspended into the critical
     * section, so no task switch can extend the pulse before fetch_bits()
     * releases the line. */
    vTaskSuspendAll();
    gpio_set_direction(DHT22_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT22_GPIO, 0);
    ets_delay_us(DHT_START_PULSE_US);

    taskENTER_CRITICAL();
    dht_result_t result = fetch_bits(bits, &bit);
    taskEXIT_CRITICAL();
    xTaskResumeAll();

    /* Idle state between reads: released, pulled up by the module. */
    gpio_set_direction(DHT22_GPIO, GPIO_MODE_INPUT);
    if (failed_bit) *failed_bit = bit;
    if (result != DHT_OK) return result;

    for (int i = 0; i < DHT_DATA_BITS; i++) {
        data[i / 8] = (data[i / 8] << 1) | bits[i];
    }
    for (int i = 0; i < 5; i++) capture.data[i] = data[i];
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) return DHT_ERR_CHECKSUM;

    *humidity_x10 = convert(data[0], data[1]);
    *temperature_x10 = convert(data[2], data[3]);
    return DHT_OK;
}

const dht_capture_t *dht22_last_capture(void)
{
    return &capture;
}
