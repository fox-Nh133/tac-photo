#include "photo_display_fs.h"
#include "ff.h"        // FatFS API
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "photo_fs";

// ファイル構造体
typedef struct {
    FIL fil;
} fs_file_t;

static void *fs_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    fs_file_t *file = malloc(sizeof(fs_file_t));
    if (!file) return NULL;

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "/sdcard/%s", path);  // "sample.jpg" → "/sdcard/sample.jpg"

    BYTE fatfs_mode = 0;
    if (mode == LV_FS_MODE_WR) fatfs_mode = FA_WRITE | FA_OPEN_ALWAYS;
    else if (mode == LV_FS_MODE_RD) fatfs_mode = FA_READ;
    else fatfs_mode = FA_READ | FA_WRITE;

    FRESULT res = f_open(&file->fil, full_path, fatfs_mode);
    if (res != FR_OK) {
        ESP_LOGE(TAG, "f_open failed: %d", res);
        free(file);
        return NULL;
    }

    return file;
}

static lv_fs_res_t fs_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    fs_file_t *file = (fs_file_t *)file_p;
    UINT bytes_read;
    FRESULT res = f_read(&file->fil, buf, btr, &bytes_read);
    *br = bytes_read;
    return (res == FR_OK) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t fs_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    fs_file_t *file = (fs_file_t *)file_p;
    FSIZE_t new_pos = 0;
    switch (whence) {
        case LV_FS_SEEK_SET: new_pos = pos; break;
        case LV_FS_SEEK_CUR: new_pos = f_tell(&file->fil) + pos; break;
        case LV_FS_SEEK_END: new_pos = f_size(&file->fil) + pos; break;
        default: return LV_FS_RES_INV_PARAM;
    }
    FRESULT res = f_lseek(&file->fil, new_pos);
    return (res == FR_OK) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t fs_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos)
{
    fs_file_t *file = (fs_file_t *)file_p;
    *pos = f_tell(&file->fil);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_close_cb(lv_fs_drv_t *drv, void *file_p)
{
    fs_file_t *file = (fs_file_t *)file_p;
    f_close(&file->fil);
    free(file);
    return LV_FS_RES_OK;
}

void photo_display_fs_init(void)
{
    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);
    drv.letter = 'S';  // "S:/sample.jpg" という形式で使えるように
    drv.open_cb = fs_open_cb;
    drv.read_cb = fs_read_cb;
    drv.seek_cb = fs_seek_cb;
    drv.tell_cb = fs_tell_cb;
    drv.close_cb = fs_close_cb;
    lv_fs_drv_register(&drv);
}
