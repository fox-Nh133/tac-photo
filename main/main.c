#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>

#include "storage_manager.h"
#include "photo_display_fs.h"
#include "photo_display.h"
#include "waveshare_rgb_lcd_port.h"

static const char *TAG = "main";

void app_main(void)
{
    // mount SD card
    esp_err_t ret = storage_mount_sdcard();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card.");
        return;
    }

    // check jpeg file existence
    const char *jpeg_path = "/sdcard/sample.jpg";
    FILE *f = fopen(jpeg_path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "JPEG file not found: %s", jpeg_path);
        return;
    }
    fclose(f);
    ESP_LOGI(TAG, "Found JPEG file: %s", jpeg_path);

    // initialize LCD and LVGL
    ret = waveshare_esp32_s3_rgb_lcd_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed");
        return;
    }

    // register the FatFS bridge for LVGL
    photo_display_fs_init();

    // process LVGL tasks
    if (lvgl_port_lock(-1)) {
        photo_display_show_image("S:/sample.jpg");
        lvgl_port_unlock();
    }

    // loop or exit if needed
}
