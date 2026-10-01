/*
 * LVGL allocator for the ESP8266 build (LV_USE_STDLIB_MALLOC ==
 * LV_STDLIB_CUSTOM, see lv_conf.h): plain malloc() on this SDK is
 * heap_caps_malloc(MALLOC_CAP_32BIT), which tries the IRAM heap region first.
 * IRAM only supports 32-bit loads/stores -- every 8/16-bit access to it traps
 * into the SDK's LoadStoreErrorHandler and is emulated in software. LVGL's
 * objects, styles, draw tasks and masks are full of byte/halfword fields, so
 * in IRAM rendering is several times slower. MALLOC_CAP_8BIT keeps LVGL in
 * DRAM; the IRAM heap stays available to everything else (WiFi, lwIP).
 */
#include "lvgl.h"

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM

#include "esp_heap_caps.h"

void lv_mem_init(void)
{
}

void lv_mem_deinit(void)
{
}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes)
{
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return NULL; /* not supported */
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    LV_UNUSED(pool);
}

void *lv_malloc_core(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_8BIT);
}

void *lv_realloc_core(void *p, size_t new_size)
{
    return heap_caps_realloc(p, new_size, MALLOC_CAP_8BIT);
}

void lv_free_core(void *p)
{
    heap_caps_free(p);
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
    LV_UNUSED(mon_p); /* not supported */
}

lv_result_t lv_mem_test_core(void)
{
    return LV_RESULT_OK;
}

#endif
