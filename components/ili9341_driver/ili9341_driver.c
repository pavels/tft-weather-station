#include "ili9341_driver.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/pwm.h"
#include "driver/spi.h"
#include "spi_bus.h"

/* Wait after init commands flagged with a delay (Sleep Out, Display On), ms:
 * lets the panel's supply and oscillator settle (the datasheet asks for >= 5ms
 * after Sleep Out; 100 is a comfortable margin). */
#define INIT_CMD_DELAY_MS 100

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; /* number of data bytes; bit 7 = delay after; 0xFF = end of list */
} lcd_init_cmd_t;

/* Gamma correction: 0-255 brightness -> 0-255 duty. */
static const uint8_t gamma8_correction[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

/* pwm_init() is given only the backlight pin, so it's channel 0. */
#define BACKLIGHT_PWM_CHANNEL 0
/* 1kHz: well above visible flicker; the duty resolution is 1us, i.e. 1000 steps. */
#define BACKLIGHT_PWM_PERIOD_US 1000

/* ESP8266 HSPI hardware FIFO is 64 bytes; every spi_trans() mosi phase must
 * fit in that, and the SDK recommends word-aligned buffers for it, hence the
 * local uint32_t staging chunk below instead of pointing spi_trans directly
 * at arbitrary (possibly unaligned) source bytes. */
#define SPI_CHUNK_BYTES 64

/* CS/DC are applied by the shared bus (spi_bus.c) at the start of each
 * transfer, never driven directly here -- see spi_bus_select(). */
static void send_cmd(uint8_t cmd)
{
    uint32_t word = cmd;
    spi_trans_t trans = {0};
    trans.mosi = &word;
    trans.bits.mosi = 8;

    spi_bus_select(ILI9341_CS_GPIO, ILI9341_DC_GPIO, 0);
    spi_trans(HSPI_HOST, &trans);
}

static void send_data(const uint8_t *data, size_t len)
{
    uint32_t chunk[SPI_CHUNK_BYTES / 4];

    spi_bus_select(ILI9341_CS_GPIO, ILI9341_DC_GPIO, 1);
    while (len > 0) {
        size_t n = len > SPI_CHUNK_BYTES ? SPI_CHUNK_BYTES : len;
        memcpy(chunk, data, n); /* spi_trans copies it into the SPI FIFO */

        spi_trans_t trans = {0};
        trans.mosi = chunk;
        trans.bits.mosi = n * 8;
        spi_trans(HSPI_HOST, &trans);

        data += n;
        len -= n;
    }
}

static void set_addr_window(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    uint8_t data[4];

    send_cmd(0x2A); /* Column address set */
    data[0] = (x1 >> 8) & 0xFF;
    data[1] = x1 & 0xFF;
    data[2] = (x2 >> 8) & 0xFF;
    data[3] = x2 & 0xFF;
    send_data(data, 4);

    send_cmd(0x2B); /* Page address set */
    data[0] = (y1 >> 8) & 0xFF;
    data[1] = y1 & 0xFF;
    data[2] = (y2 >> 8) & 0xFF;
    data[3] = y2 & 0xFF;
    send_data(data, 4);

    send_cmd(0x2C); /* Memory write */
}

void ili9341_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ILI9341_CS_GPIO) | (1ULL << ILI9341_DC_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(ILI9341_CS_GPIO, 1);

    const uint32_t backlight_pin = ILI9341_BCKL_GPIO;
    uint32_t backlight_duty = 0;
    pwm_init(BACKLIGHT_PWM_PERIOD_US, &backlight_duty, 1, &backlight_pin);
    /* pwm_init() leaves the phase uninitialized (malloc'd, not zeroed), so
     * pwm_start() rejects it with "phase error" -- set it, as the SDK's own
     * PWM example does. */
    pwm_set_phase(BACKLIGHT_PWM_CHANNEL, 0);
    pwm_start();

    const lcd_init_cmd_t init_cmds[] = {
        {0xEF, {0x03, 0x80, 0X02}, 3},
        {0xCF, {0x00, 0xC1, 0X30}, 3},
        {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
        {0xE8, {0x85, 0x00, 0x78}, 3},
        {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
        {0xF7, {0x20}, 1},
        {0xEA, {0x00, 0x00}, 2},
        {0xC0, {0x23}, 1},         /* Power control */
        {0xC1, {0x10}, 1},         /* Power control */
        {0xC5, {0x3E, 0x28}, 2},   /* VCOM control */
        {0xC7, {0x86}, 1},         /* VCOM control */
        {0x36, {0x48}, 1},         /* Memory Access Control */
        {0x3A, {0x55}, 1},         /* Pixel Format Set: 16bpp */
        {0xB6, {0x08, 0x82, 0x27}, 3},
        {0xF2, {0x00}, 1},         /* 3Gamma Function Disable */
        {0x26, {0x01}, 1},         /* Gamma Set */
        {0xE0, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}, 15},
        {0XE1, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}, 15},
        {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
        {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
        {0x11, {0}, 0x80}, /* Sleep out, then delay */
        {0x29, {0}, 0x80}, /* Display on, then delay */
        {0, {0}, 0xFF},    /* end of list */
    };

    for (int i = 0; init_cmds[i].databytes != 0xFF; i++) {
        send_cmd(init_cmds[i].cmd);
        send_data(init_cmds[i].data, init_cmds[i].databytes & 0x1F);
        if (init_cmds[i].databytes & 0x80) {
            vTaskDelay(pdMS_TO_TICKS(INIT_CMD_DELAY_MS));
        }
    }
}

void ili9341_set_backlight(uint8_t brightness)
{
    /* The SDK's PWM duty is in microseconds of the period, not 0-255. */
    pwm_set_duty(BACKLIGHT_PWM_CHANNEL,
                 (uint32_t)gamma8_correction[brightness] * BACKLIGHT_PWM_PERIOD_US / 255);
    pwm_start();
}

void ili9341_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color)
{
    uint8_t pattern[SPI_CHUNK_BYTES];
    for (size_t i = 0; i < sizeof(pattern); i += 2) {
        pattern[i] = color >> 8;
        pattern[i + 1] = color & 0xFF;
    }

    set_addr_window(x, y, x + w - 1, y + h - 1);
    size_t remaining = (size_t)w * h * 2;
    while (remaining > 0) {
        size_t n = remaining > sizeof(pattern) ? sizeof(pattern) : remaining;
        send_data(pattern, n);
        remaining -= n;
    }
}

void ili9341_begin_pixels(int32_t x, int32_t y, int32_t w, int32_t h)
{
    set_addr_window(x, y, x + w - 1, y + h - 1);
}

void ili9341_write_pixels(const uint8_t *data, size_t len)
{
    send_data(data, len);
}

void ili9341_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    set_addr_window(area->x1, area->y1, area->x2, area->y2);

    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    send_data(px_map, (size_t)w * h * 2 /* RGB565 = 2 bytes/px */);

    lv_display_flush_ready(disp);
}
