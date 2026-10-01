# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# ESP8266_RTOS_SDK v3.4 (and some of its vendored submodules, e.g. mbedtls)
# predate CMake 3.5's minimum-version floor; CMake 4.x removed support for
# anything below that outright. This tells CMake to treat those old
# `cmake_minimum_required` calls as if they said 3.5, instead of erroring.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# app/ is the shared platform-agnostic component, also compiled directly by
# simulator/CMakeLists.txt for the host build.
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/app)

project(tft_weather_station)
