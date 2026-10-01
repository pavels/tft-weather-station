#include <stdio.h>
#include <unistd.h>

#define SDL_MAIN_HANDLED /* avoid SDL's "undefined reference to WinMain" */
#include <SDL2/SDL.h>

#include "lvgl.h"

#include "Controller.h"

/* Main loop period, ms: how often lv_timer_handler() runs. */
#define LOOP_PERIOD_MS 5

void platform_check_for_update(bool power_off_hours)
{
    (void)power_off_hours;
}

void platform_manual_update(void)
{
    printf("firmware updates are not supported in the simulator\n");
}

void platform_confirm_boot(void)
{
}

int main(void)
{
    lv_init();
    lv_tick_set_cb(SDL_GetTicks);

    lv_sdl_window_create(240, 320); /* panel is mounted portrait */
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();

    /* The host already has network access; no WiFi association to wait for. */
    wifi_connected = true;

    initialize_tft_station();

    while (1) {
        lv_timer_handler();
        usleep(LOOP_PERIOD_MS * 1000);
    }

    return 0;
}
