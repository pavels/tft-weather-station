#ifndef SPI_BUS_H_
#define SPI_BUS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* No DC line (e.g. the touch controller). */
#define SPI_BUS_NO_DC (-1)

/*
 * The HSPI bus shared by the display (ili9341_driver) and the touch
 * controller (xpt2046_driver). Brings the bus up at 8MHz with software CS,
 * and installs the bus's one SPI event callback.
 */
void spi_bus_init(void);

/*
 * Selects the device (and, for the display, its DC level) that the next
 * spi_trans() calls talk to. Call before each transfer or run of transfers.
 *
 * The pins aren't driven here: spi_trans() returns while a write-only
 * transfer is still shifting out, so changing CS/DC directly would cut it
 * short (seen on the panel as stale pixel runs). The SPI driver's
 * SPI_TRANS_START_EVENT applies them instead -- it's raised only once the
 * previous transfer has finished, right before the next one starts, as in
 * the SDK's spi_oled example. The previously selected device's CS is
 * released at that point; the selected one stays asserted until another
 * device takes the bus.
 *
 * cs_gpio must already be configured as an output (idle high).
 */
void spi_bus_select(int cs_gpio, int dc_gpio, uint32_t dc_level);

#ifdef __cplusplus
}
#endif

#endif /* SPI_BUS_H_ */
