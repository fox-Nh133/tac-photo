#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>
#include "storage_manager.h"

static const char *TAG = "main";

void app_main(void)
{
    esp_err_t ret = storage_mount_sdcard();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card.");
        return;
    }

    const char *jpeg_path = "/sdcard/sample.jpg";
    FILE *f = fopen(jpeg_path, "rb");
    if (f) {
        ESP_LOGI(TAG, "Found JPEG file: %s", jpeg_path);
        fclose(f);
    } else {
        ESP_LOGW(TAG, "JPEG file not found: %s", jpeg_path);
    }

    // unmount if needed
    // storage_unmount_sdcard();
}
