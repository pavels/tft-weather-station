#ifndef OTA_UPDATE_H_
#define OTA_UPDATE_H_

#include <stdbool.h>

/*
 * Trial boot of a freshly installed image: call right after NVS is up, before
 * anything else can crash. Counts the image's unconfirmed boots; after
 * MAX_TRIAL_BOOTS it switches back to the previous slot and restarts.
 */
void ota_trial_boot_check(void);

/* The running image works (WiFi and weather loaded): ends its trial. Cheap to
 * call repeatedly. */
void ota_confirm_boot(void);

/* Hourly check (normal mode, plain HTTP): restarts into update mode if the
 * version service announces a newer release. `power_off_hours` allows the
 * fallback when the service has been unreachable for days. */
void ota_check_for_update(bool power_off_hours);

/* True if the app should boot into update mode instead of the weather UI. */
bool ota_update_requested(void);

/*
 * Restarts into update mode. `version` is the version the hourly check
 * announced, or NULL for a manual check; returns without restarting if that
 * announced version already failed too often.
 */
void ota_request_update(const char *version);

/*
 * Update mode: draws the update screen straight to the panel (no LVGL, to
 * leave the RAM to TLS) and starts the update task, which fetches the signed
 * manifest, installs the release into the other app slot if it is newer, and
 * restarts. Call after spi_bus_init() and ili9341_init(); WiFi is started by
 * the caller.
 */
void ota_update_mode_start(void);

#endif /* OTA_UPDATE_H_ */
