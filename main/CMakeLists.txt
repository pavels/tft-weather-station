# pthread: the SDK pulls libstdc++ in via NVS (C++), and its static-init guard
# needs pthread_cond_*. With an explicit REQUIRES list, pthread isn't in the
# linker's rescanned group, and the SDK's own "-u" to force it in has a typo
# (..._cond_impl vs. the real ..._cond_var_impl), so it must be listed here.
idf_component_register(SRCS "app_main.c"
                    INCLUDE_DIRS ""
                    REQUIRES lvgl ili9341_driver xpt2046_driver dht_driver spi_bus app ota_update pthread nvs_flash esp_event tcpip_adapter)

# newlib-nano (CONFIG_NEWLIB_NANO_FORMAT, the SDK default) leaves float support
# out of printf unless _printf_float is force-linked: without it every "%f"
# formats as nothing.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u _printf_float")
