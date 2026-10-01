#include <string.h>

#include "IconFs.h"

#if(TARGET_ESP8266 == 1)
#include "esp_log.h"
#include "esp_partition.h"
#else
#include <stdio.h>
#endif

/*
 * Read-only LVGL drive "I:" over the icons.bin blob built by
 * resources/gen_icon_blob.py (see its header for the layout): "I:<name>.bin"
 * opens one icon, which LVGL's bin decoder then reads row by row. Only the
 * blob backend differs per platform -- the "icons" flash partition on the
 * ESP8266, a plain file on the simulator -- so both exercise the same path.
 */

/* icons.bin layout, must match resources/gen_icon_blob.py: 4-byte magic,
 * u32 icon count, then one index entry per icon -- a NUL-padded name of
 * NAME_LEN bytes, u32 offset, u32 size. The index starts at byte 8. */
#define BLOB_MAGIC "WICO"
#define NAME_LEN 24
#define INDEX_ENTRY_SIZE (NAME_LEN + 8)

typedef struct {
	uint32_t base; /* icon's offset within the blob */
	uint32_t size;
	uint32_t pos;
} icon_file_t;

static uint32_t icon_count;

#if(TARGET_ESP8266 == 1)
static const char *TAG = "icon_fs";
static const esp_partition_t *icons_partition;

static bool blob_open(void) {
	icons_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "icons");
	if (icons_partition == NULL) {
		ESP_LOGE(TAG, "no \"icons\" partition -- check partitions.csv");
	}
	return icons_partition != NULL;
}

static bool blob_read(uint32_t offset, void *buf, uint32_t len) {
	return esp_partition_read(icons_partition, offset, buf, len) == ESP_OK;
}

static void blob_error(const char *msg) {
	ESP_LOGE(TAG, "%s -- was icons.bin flashed? (idf.py flash does it)", msg);
}
#else
static FILE *blob_file;

static bool blob_open(void) {
	blob_file = fopen(ICONS_BIN_PATH, "rb");
	if (blob_file == NULL) {
		fprintf(stderr, "icon_fs: cannot open %s\n", ICONS_BIN_PATH);
	}
	return blob_file != NULL;
}

static bool blob_read(uint32_t offset, void *buf, uint32_t len) {
	return fseek(blob_file, (long)offset, SEEK_SET) == 0 && fread(buf, 1, len, blob_file) == len;
}

static void blob_error(const char *msg) {
	fprintf(stderr, "icon_fs: %s (%s)\n", msg, ICONS_BIN_PATH);
}
#endif

static void *icon_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode) {
	(void)drv;
	if (mode != LV_FS_MODE_RD) return NULL;

	/* LVGL passes the path without the "I:" prefix; the blob stores names
	 * without the ".bin" the bin decoder insists on. */
	size_t name_len = strlen(path);
	if (name_len > 4 && strcmp(path + name_len - 4, ".bin") == 0) name_len -= 4;
	if (name_len >= NAME_LEN) return NULL;

	for (uint32_t i = 0; i < icon_count; i++) {
		uint8_t entry[INDEX_ENTRY_SIZE];
		if (!blob_read(8 + i * INDEX_ENTRY_SIZE, entry, sizeof(entry))) return NULL;
		if (strncmp((const char *)entry, path, name_len) != 0 || entry[name_len] != '\0') continue;

		icon_file_t *file = lv_malloc(sizeof(icon_file_t));
		if (file == NULL) return NULL;
		memcpy(&file->base, entry + NAME_LEN, 4);
		memcpy(&file->size, entry + NAME_LEN + 4, 4);
		file->pos = 0;
		return file;
	}
	return NULL;
}

static lv_fs_res_t icon_close(lv_fs_drv_t *drv, void *file_p) {
	(void)drv;
	lv_free(file_p);
	return LV_FS_RES_OK;
}

static lv_fs_res_t icon_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br) {
	(void)drv;
	icon_file_t *file = file_p;
	uint32_t left = file->size - file->pos;
	if (btr > left) btr = left;
	if (btr > 0 && !blob_read(file->base + file->pos, buf, btr)) {
		*br = 0;
		return LV_FS_RES_HW_ERR;
	}
	file->pos += btr;
	*br = btr;
	return LV_FS_RES_OK;
}

static lv_fs_res_t icon_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence) {
	(void)drv;
	icon_file_t *file = file_p;
	uint32_t target;
	switch (whence) {
	case LV_FS_SEEK_SET: target = pos; break;
	case LV_FS_SEEK_CUR: target = file->pos + pos; break;
	case LV_FS_SEEK_END: target = file->size + pos; break;
	default: return LV_FS_RES_INV_PARAM;
	}
	if (target > file->size) return LV_FS_RES_INV_PARAM;
	file->pos = target;
	return LV_FS_RES_OK;
}

static lv_fs_res_t icon_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p) {
	(void)drv;
	*pos_p = ((icon_file_t *)file_p)->pos;
	return LV_FS_RES_OK;
}

void icon_fs_init(void) {
	static lv_fs_drv_t drv;

	if (!blob_open()) return;
	uint8_t head[8];
	if (!blob_read(0, head, sizeof(head)) || memcmp(head, BLOB_MAGIC, 4) != 0) {
		blob_error("icon blob missing or corrupt, icons won't show");
		return;
	}
	memcpy(&icon_count, head + 4, 4);

	lv_fs_drv_init(&drv);
	drv.letter = ICON_FS_LETTER;
	drv.open_cb = icon_open;
	drv.close_cb = icon_close;
	drv.read_cb = icon_read;
	drv.seek_cb = icon_seek;
	drv.tell_cb = icon_tell;
	lv_fs_drv_register(&drv);
}
