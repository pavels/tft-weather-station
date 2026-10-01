#include "spi_bus.h"

#include "driver/gpio.h"
#include "driver/spi.h"
#include "esp_attr.h"

/* What the next transfer should use (set by spi_bus_select), and which CS
 * is actually asserted right now (updated only in the event callback). */
static volatile int next_cs = -1;
static volatile int next_dc_gpio = SPI_BUS_NO_DC;
static volatile uint32_t next_dc_level;
static volatile int active_cs = -1;

static void IRAM_ATTR spi_event_cb(int event, void *arg)
{
    if (event != SPI_TRANS_START_EVENT) return;

    if (active_cs != next_cs) {
        if (active_cs >= 0) gpio_set_level(active_cs, 1);
        active_cs = next_cs;
    }
    if (next_dc_gpio != SPI_BUS_NO_DC) gpio_set_level(next_dc_gpio, next_dc_level);
    if (active_cs >= 0) gpio_set_level(active_cs, 0);
}

void spi_bus_init(void)
{
    spi_config_t spi_cfg = {0};
    spi_cfg.interface.val = SPI_DEFAULT_INTERFACE;
    spi_cfg.interface.cs_en = 0;   /* CS is per device, driven in spi_event_cb */
    spi_cfg.interface.miso_en = 1; /* the touch controller is read back */
    spi_cfg.interface.cpol = SPI_CPOL_LOW;
    spi_cfg.interface.cpha = SPI_CPHA_LOW;
    spi_cfg.intr_enable.val = SPI_MASTER_DEFAULT_INTR_ENABLE;
    spi_cfg.mode = SPI_MASTER_MODE;
    spi_cfg.clk_div = SPI_8MHz_DIV;
    spi_cfg.event_cb = spi_event_cb;
    spi_init(HSPI_HOST, &spi_cfg);
}

void spi_bus_select(int cs_gpio, int dc_gpio, uint32_t dc_level)
{
    next_cs = cs_gpio;
    next_dc_gpio = dc_gpio;
    next_dc_level = dc_level;
}
