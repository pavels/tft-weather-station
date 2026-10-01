# Thin ESP8266_RTOS_SDK component wrapper around the lvgl/lvgl submodule.
#
# LVGL's own CMakeLists.txt (env_support/cmake/esp.cmake) targets modern
# ESP-IDF's Kconfig-driven component config and PRIV_REQUIRES an "esp_timer"
# component that doesn't exist as such in this older SDK (esp_timer.h lives
# inside the esp8266 component here) -- so we register it ourselves instead,
# using our own hand-maintained lv_conf.h (../../lvgl/lv_conf.h) rather than
# LVGL's Kconfig config path.

set(LVGL_DIR "${CMAKE_CURRENT_LIST_DIR}/../../lvgl/lvgl")
set(LVGL_CONF_DIR "${CMAKE_CURRENT_LIST_DIR}/../../lvgl")

file(GLOB_RECURSE LVGL_SOURCES "${LVGL_DIR}/src/*.c")

idf_component_register(
    SRCS ${LVGL_SOURCES} "lv_mem_core_esp8266.c"
    INCLUDE_DIRS "${LVGL_DIR}" "${LVGL_CONF_DIR}"
    PRIV_REQUIRES heap
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC LV_CONF_INCLUDE_SIMPLE)
