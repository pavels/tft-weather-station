# Generated fonts (see resources/), compiled in alongside app/ on both builds.
# The icons are not: they're packed into icons.bin below instead.
file(GLOB RESOURCE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../resources/fonts/*.c")

# Shared platform-agnostic logic, compiled into both the ESP8266_RTOS_SDK
# build (as this component) and the simulator build (simulator/CMakeLists.txt
# compiles these same .c files directly).
idf_component_register(
    SRCS
        "src/JsonStream.c"
        "src/JsonFeed.c"
        "src/HttpClient.c"
        "src/Lang.c"
        "src/WeatherIcons.c"
        "src/City.c"
        "src/Weather.c"
        "src/Clock.c"
        "src/CustomStyles.c"
        "src/Widgets.c"
        "src/Config.c"
        "src/MainScreens.c"
        "src/SplashScreen.c"
        "src/ConfigScreen.c"
        "src/Controller.c"
        "src/IconFs.c"
        "../third_party/http_parser/http_parser.c"
        ${RESOURCE_SOURCES}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS ".." "../third_party/http_parser" "../resources"
    REQUIRES lvgl
    PRIV_REQUIRES freertos ili9341_driver nvs_flash spi_flash
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC TARGET_ESP8266=1)
# lv_font_conv output includes "lvgl/lvgl.h" unless this is set.
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)

# Weather icons -> icons.bin, flashed into the "icons" partition by `idf.py
# flash` (same pattern as the SDK's own phy_init_data image).
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    idf_build_get_property(build_dir BUILD_DIR)
    idf_build_get_property(python PYTHON)
    set(icon_blob_script "${CMAKE_CURRENT_LIST_DIR}/../resources/gen_icon_blob.py")
    file(GLOB icon_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../resources/icons/*/*.c")
    set(icons_bin "${build_dir}/icons.bin")

    add_custom_command(OUTPUT ${icons_bin}
        COMMAND ${python} ${icon_blob_script} ${icons_bin} ${icon_sources}
        DEPENDS ${icon_blob_script} ${icon_sources}
        COMMENT "Packing weather icons into icons.bin")
    add_custom_target(icons_bin ALL DEPENDS ${icons_bin})
    add_dependencies(flash icons_bin)

    partition_table_get_partition_info(icons_offset "--partition-name icons" "offset")
    esptool_py_flash_project_args(icons ${icons_offset} ${icons_bin} FLASH_IN_PROJECT)
endif()
