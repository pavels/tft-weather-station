#ifndef OTA_UPDATE_UPDATE_SCREEN_H_
#define OTA_UPDATE_UPDATE_SCREEN_H_

/*
 * Update mode's screen: a title, a progress bar and a status line, drawn
 * straight to the panel through the ILI9341 driver -- no LVGL, no frame buffer,
 * to leave the RAM to TLS. Needs spi_bus_init() and ili9341_init() first.
 */
void update_screen_init(void);

/* percent: 0-100. Drawing it smaller than before restarts the bar. */
void update_screen_set_progress(int percent);

void update_screen_set_status(const char *status);

#endif /* OTA_UPDATE_UPDATE_SCREEN_H_ */
