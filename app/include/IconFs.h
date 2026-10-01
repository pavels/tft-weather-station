#ifndef APP_ICONFS_H_
#define APP_ICONFS_H_

#include "lvgl.h"

/* Drive letter the weather icons are served under: "I:wi_sunny_big.bin". */
#define ICON_FS_LETTER 'I'

/* Registers the icon drive with LVGL; call after lv_init(). If the blob is
 * missing it logs an error and icons simply don't render. */
void icon_fs_init(void);

#endif /* APP_ICONFS_H_ */
