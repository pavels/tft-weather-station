# Standalone host-native build, not part of idf.py. Compiles lvgl/lvgl/src
# directly (like components/lvgl) instead of using LVGL's own CMakeLists.txt,
# which also builds examples/demos/ThorVG.
cmake_minimum_required(VERSION 3.12) # CONFIGURE_DEPENDS
project(tft_weather_station_sim C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(SDL2 REQUIRED)

set(LVGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lvgl/lvgl")
file(GLOB_RECURSE LVGL_SOURCES "${LVGL_DIR}/src/*.c")

set(APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../app")
set(HTTP_PARSER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/http_parser")
# CONFIGURE_DEPENDS: re-checked on every build, so added or removed files are
# picked up without re-running cmake by hand.
file(GLOB APP_SOURCES CONFIGURE_DEPENDS "${APP_DIR}/src/*.c")
set(RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../resources")
# Fonts are compiled in; icons are packed into icons.bin and read through the
# same "I:" drive (app/src/IconFs.c) as on the device, just from a file.
file(GLOB RESOURCE_SOURCES CONFIGURE_DEPENDS "${RESOURCES_DIR}/fonts/*.c")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB ICON_SOURCES CONFIGURE_DEPENDS "${RESOURCES_DIR}/icons/*/*.c")
set(ICONS_BIN "${CMAKE_CURRENT_BINARY_DIR}/icons.bin")
add_custom_command(OUTPUT ${ICONS_BIN}
    COMMAND Python3::Interpreter "${RESOURCES_DIR}/gen_icon_blob.py" ${ICONS_BIN} ${ICON_SOURCES}
    DEPENDS "${RESOURCES_DIR}/gen_icon_blob.py" ${ICON_SOURCES}
    COMMENT "Packing weather icons into icons.bin")
add_custom_target(icons_bin DEPENDS ${ICONS_BIN})

add_executable(tft_weather_station_sim
    main.c
    ${LVGL_SOURCES}
    ${APP_SOURCES}
    ${RESOURCE_SOURCES}
    "${HTTP_PARSER_DIR}/http_parser.c"
)

target_include_directories(tft_weather_station_sim PRIVATE
    "${LVGL_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}" # simulator/lv_conf.h
    "${APP_DIR}/include"
    "${HTTP_PARSER_DIR}"
    "${RESOURCES_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.." # credentials.h
)

target_compile_definitions(tft_weather_station_sim PRIVATE LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE
    ICONS_BIN_PATH="${ICONS_BIN}")
add_dependencies(tft_weather_station_sim icons_bin)
target_link_libraries(tft_weather_station_sim PRIVATE SDL2::SDL2)
