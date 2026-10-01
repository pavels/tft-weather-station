#include "xpt2046_driver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/spi.h"
#include "esp_log.h"
#include "spi_bus.h"

/*
 * Each sample is one half-duplex transfer: 8 control bits out, then 16 bits
 * in. The value is (hi << 4) | (lo >> 4) of those 16 bits, which keeps the
 * converter's leading null bit, so values span 0..2047 -- the range the
 * calibration constants below are in.
 */

static const char *TAG = "xpt2046";

/* Number of calibration points (the four TOUCHCAL_* corners). */
#define SAMPLE_POINTS 4
#define MAX_SAMPLES 0xff /* per axis, until two consecutive reads agree */

/* Settle time after configuring the pins, before the first conversion, ms. */
#define POWER_UP_DELAY_MS 10

/* Control byte: S A2-A0 (high nibble), MODE SER/DFR PD1 PD0 (low nibble). */
#define XPT2046_CTRL_LO_DFR 0x3 /* differential, ADC stays powered */
#define XPT2046_CTRL_LO_SER 0x4 /* power down between conversions, PENIRQ enabled */
#define XPT2046_CTRL_HI_X (0x9 << 4) /* S=1, A=001: position along the sensor's X axis */
#define XPT2046_CTRL_HI_Y (0xD << 4) /* S=1, A=101: position along the sensor's Y axis */

/* Panel resolution in portrait, px. The sensor is mounted rotated 90 degrees
 * relative to it, so the sensor's X spans the panel's 320px height. */
#define DISP_HOR_RESOLUTION 240
#define DISP_VER_RESOLUTION 320
#define GET_MAX_X() (DISP_VER_RESOLUTION - 1)
#define GET_MAX_Y() (DISP_HOR_RESOLUTION - 1)

/* How far in from the screen edges the calibration points lie: 20% of the
 * screen size in total, i.e. 10% per side (valid range 0..40). */
#define CALIBRATION_INSET 20

/* Raw ADC readings (0..2047) at the four calibration points: upper-left,
 * upper-right, lower-left, lower-right, CALIBRATION_INSET in from the edges. */
#define TOUCHCAL_ULX 360
#define TOUCHCAL_ULY 354
#define TOUCHCAL_URX 1856
#define TOUCHCAL_URY 358
#define TOUCHCAL_LLX 365
#define TOUCHCAL_LLY 1738
#define TOUCHCAL_LRX 1845
#define TOUCHCAL_LRY 1738

/* Transform coefficients are fixed point with CAL_SCALE_BITS fractional bits,
 * to avoid floating point. CAL_*_INSET is CALIBRATION_INSET's per-side share
 * in px, i.e. where the calibration points are on screen. */
#define CAL_SCALE_BITS 8
#define CAL_SCALE (1 << CAL_SCALE_BITS)
#define CAL_X_INSET (((GET_MAX_X() + 1) * (CALIBRATION_INSET >> 1)) / 100)
#define CAL_Y_INSET (((GET_MAX_Y() + 1) * (CALIBRATION_INSET >> 1)) / 100)

/* Transform coefficients: y = (A * rawY + B) >> bits, x = (C * rawX + D) >> bits. */
static long tr_a, tr_b, tr_c, tr_d;

/* One conversion: send `ctrl`, read back the 16 bits that follow. */
static uint16_t xpt2046_sample(uint8_t ctrl)
{
    uint32_t tx = ctrl;
    uint32_t rx = 0; /* word-aligned: the SDK warns (and copies) otherwise */
    spi_trans_t trans = {0};
    trans.mosi = &tx;
    trans.bits.mosi = 8;
    trans.miso = &rx;
    trans.bits.miso = 16;

    spi_bus_select(XPT2046_CS_GPIO, SPI_BUS_NO_DC, 0);
    spi_trans(HSPI_HOST, &trans); /* waits for completion when reading */

    /* Bytes land in the word in wire order, lowest address first -- the same
     * order the display relies on for sending. */
    uint8_t hi = rx & 0xFF;
    uint8_t lo = (rx >> 8) & 0xFF;
    return ((uint16_t)hi << 4) | (lo >> 4);
}

static uint16_t xpt2046_read_axis(uint8_t ctrl)
{
    uint16_t prev, cur = 0xffff;
    uint8_t i = 0;
    do {
        prev = cur;
        cur = xpt2046_sample(ctrl);
    } while (prev != cur && ++i < MAX_SAMPLES);
    return cur;
}

static void calculate_calibration(void)
{
    const int16_t x_raw[SAMPLE_POINTS] = {TOUCHCAL_ULX, TOUCHCAL_URX, TOUCHCAL_LRX, TOUCHCAL_LLX};
    const int16_t y_raw[SAMPLE_POINTS] = {TOUCHCAL_ULY, TOUCHCAL_URY, TOUCHCAL_LRY, TOUCHCAL_LLY};
    int16_t x_point[SAMPLE_POINTS], y_point[SAMPLE_POINTS];
    long a, b, c, d, a0, b0, c0, d0;
    long test1, test2;

    y_point[0] = y_point[1] = CAL_Y_INSET;
    y_point[2] = y_point[3] = GET_MAX_Y() - CAL_Y_INSET;
    x_point[0] = x_point[3] = CAL_X_INSET;
    x_point[1] = x_point[2] = GET_MAX_X() - CAL_X_INSET;

    /* Solve with sample points 1 and 4 (and 1 and 3 for x)... */
    test1 = (long)y_point[0] - y_point[3];
    test2 = (long)y_raw[0] - y_raw[3];
    a0 = (test1 * CAL_SCALE) / test2;
    b0 = (long)y_point[0] * CAL_SCALE - a0 * y_raw[0];

    test1 = (long)x_point[0] - x_point[2];
    test2 = (long)x_raw[0] - x_raw[2];
    c0 = (test1 * CAL_SCALE) / test2;
    d0 = (long)x_point[0] * CAL_SCALE - c0 * x_raw[0];

    /* ...then with points 2 and 3 (2 and 4 for x), and average the two. */
    test1 = (long)y_point[1] - y_point[2];
    test2 = (long)y_raw[1] - y_raw[2];
    a = (test1 * CAL_SCALE) / test2;
    b = (long)y_point[1] * CAL_SCALE - a * y_raw[1];

    test1 = (long)x_point[1] - x_point[3];
    test2 = (long)x_raw[1] - x_raw[3];
    c = (test1 * CAL_SCALE) / test2;
    d = (long)x_point[1] * CAL_SCALE - c * x_raw[1];

    tr_a = (a + a0) >> 1;
    tr_b = (b + b0) >> 1;
    tr_c = (c + c0) >> 1;
    tr_d = (d + d0) >> 1;
}

void xpt2046_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << XPT2046_CS_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(XPT2046_CS_GPIO, 1);

    io_conf.pin_bit_mask = 1ULL << XPT2046_IRQ_GPIO;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE; /* PENIRQ is open drain */
    gpio_config(&io_conf);

    /* +1: vTaskDelay(n) can return after only n-1 full ticks. */
    vTaskDelay(pdMS_TO_TICKS(POWER_UP_DELAY_MS) + 1);

    /* Power-down mode with PENIRQ enabled. */
    xpt2046_sample(XPT2046_CTRL_HI_Y | XPT2046_CTRL_LO_SER);

    calculate_calibration();
}

void xpt2046_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    static int32_t last_x, last_y;

    if (gpio_get_level(XPT2046_IRQ_GPIO) != 0) {
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    long raw_x = xpt2046_read_axis(XPT2046_CTRL_HI_X | XPT2046_CTRL_LO_DFR);
    long raw_y = xpt2046_read_axis(XPT2046_CTRL_HI_Y | XPT2046_CTRL_LO_DFR);
    /* Back to power-down with PENIRQ enabled (DFR mode keeps the ADC on,
     * which disables PENIRQ). */
    xpt2046_sample(XPT2046_CTRL_HI_Y | XPT2046_CTRL_LO_SER);

    /* Calibrated sensor coordinates, then swapped/flipped for the 90-degree
     * mounting. */
    long sensor_x = raw_x > 0 ? (tr_c * raw_x + tr_d) >> CAL_SCALE_BITS : raw_x;
    long sensor_y = raw_y > 0 ? (tr_a * raw_y + tr_b) >> CAL_SCALE_BITS : raw_y;
    last_x = sensor_y;
    last_y = DISP_VER_RESOLUTION - sensor_x;

#if XPT2046_DEBUG_LOG
    ESP_LOGI(TAG, "raw %ld,%ld -> screen %d,%d", raw_x, raw_y, (int)last_x, (int)last_y);
#else
    (void)TAG;
#endif

    data->point.x = last_x;
    data->point.y = last_y;
    data->state = LV_INDEV_STATE_PRESSED;
}
