# OTA updates: hourly version check, update mode (signed manifest, download,
# install) and trial boot. ESP8266 only; the simulator doesn't build it.
idf_component_register(SRCS "ota_update.c" "update_check.c" "update_screen.c" "update_font.c"
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS "../.."
                    PRIV_REQUIRES app lvgl ili9341_driver esp_http_client app_update spi_flash mbedtls nvs_flash
                    # Release signing public key, checked against the manifest.
                    EMBED_TXTFILES "ota_public_key.pem")

# update_font.c (lv_font_conv output) includes "lvgl/lvgl.h" unless this is set.
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
