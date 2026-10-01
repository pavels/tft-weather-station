# TFT Weather Station

ESP8266 + ILI9341 (240x320, portrait) touch-screen weather station, showing current
conditions and a 3-day forecast from [Open-Meteo](https://open-meteo.com). Built on the
official **ESP8266_RTOS_SDK v3.4** and **LVGL v9**. It also builds as a PC simulator
(SDL2) that shares all UI and logic code with the device.

## Environment setup

Tested on Linux. You need two things: the ESP8266 SDK and its toolchain for the device,
and a normal host C toolchain with SDL2 for the simulator.

### 1. Host packages

```sh
# Debian/Ubuntu
sudo apt install git wget flex bison gperf python3 python3-venv python3-pip \
                 cmake ninja-build ccache libffi-dev libssl-dev \
                 build-essential libsdl2-dev
```

`libsdl2-dev` is only needed for the simulator. Node.js (`npx`) is only needed to
regenerate fonts (`resources/generate_fonts.sh`).

### 2. ESP8266_RTOS_SDK v3.4

Clone the `release/v3.4` branch, not the `v3.4` tag. The branch carries later bugfixes,
and this project is built against it.

```sh
mkdir -p ~/esp && cd ~/esp
git clone -b release/v3.4 https://github.com/espressif/ESP8266_RTOS_SDK.git
cd ESP8266_RTOS_SDK
git submodule update --init
```

### 3. Toolchain and Python environment

The SDK's installer downloads the matching Xtensa toolchain (`esp-2020r3`, GCC 8.4.0)
into `~/.espressif/` and creates a Python virtualenv with the SDK's requirements:

```sh
cd ~/esp/ESP8266_RTOS_SDK
./install.sh
```

Then pin `setuptools`. The SDK's `requirements.txt` leaves it unpinned, and newer releases
no longer ship `pkg_resources`, which the SDK's dependency check imports. Without the pin,
`export.sh` fails with `pkg_resources cannot be imported ...`. Version 80.x is known to work:

```sh
~/.espressif/python_env/rtos3.4_py3*_env/bin/pip install "setuptools<81"
```

### 4. Per shell: `IDF_PATH` and `PATH`

In every shell you build from, source the SDK's export script. It sets `IDF_PATH` and puts
the toolchain, `idf.py` and the Python env on `PATH`:

```sh
. ~/esp/ESP8266_RTOS_SDK/export.sh
```

To make it persistent, add an alias to `~/.bashrc`, e.g.
`alias get_esp8266='. ~/esp/ESP8266_RTOS_SDK/export.sh'`. Sourcing it unconditionally
from `~/.bashrc` works too, but it's slow.

Check that it worked:

```sh
echo $IDF_PATH                 # -> .../ESP8266_RTOS_SDK
xtensa-lx106-elf-gcc --version # -> 8.4.0
```

### 5. This project

```sh
git clone <this repo> tft_weather_station
cd tft_weather_station
git submodule update --init    # LVGL v9 (lvgl/lvgl); don't use --recursive, LVGL doesn't need it
cp credentials.h.example credentials.h
```

`credentials.h` is gitignored. It holds optional bench WiFi settings (see below); the
build needs the file to exist.

## Building

### Device firmware

```sh
. ~/esp/ESP8266_RTOS_SDK/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # Ctrl+] exits the monitor
```

`idf.py flash` writes the app and also `icons.bin`, the weather icons, which live in
their own flash partition (see `partitions.csv`). `idf.py app-flash` writes only the app, so
use it after the icons have been flashed once.

For a bench board without touch, WiFi can come from `credentials.h`: uncomment `WIFI_SSID` and
`WIFI_PASSWORD` there. They're only used when the device has no WiFi config saved yet.

`sdkconfig` is generated from the checked-in `sdkconfig.defaults` on the first build, and
is gitignored. Use `idf.py menuconfig` to change it locally.

### Simulator

The simulator is a standalone host CMake project, not part of `idf.py`, and needs no
ESP8266 SDK:

```sh
cmake -S simulator -B simulator/build
cmake --build simulator/build -j
./simulator/build/tft_weather_station_sim
```

It opens a 240x320 window. The mouse stands in for touch, and it fetches real weather
data using the host's network.
