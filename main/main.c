/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>

#include "storage_manager.h"

#include "waveshare_rgb_lcd_port.h"

# define SD_MOUNT_RETRIES 3
# define SD_RETRY_DELAY_MS 1000

void app_main()
{
    esp_err_t ret = ESP_FAIL;
    vTaskDelay(pdMS_TO_TICKS(SD_RETRY_DELAY_MS));

    for (int attempt = 1; attempt <= SD_MOUNT_RETRIES; ++attempt){
        ESP_LOGI(TAG, "Attempting to mount sd card (attempt %d)", attempt);
        ret = storage_mount_sdcard();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "SD card mounted successfully on attempt %d", attempt);
            // check jpeg file existence
            const char *jpeg_path = "/sdcard/sample.jpg";
            FILE *f = fopen(jpeg_path, "rb");
            if (!f) {
                ESP_LOGW(TAG, "JPEG file not found: %s", jpeg_path);
                return;
            }
            fclose(f);
            ESP_LOGI(TAG, "Found JPEG file: %s", jpeg_path);
            break;
        } else {
            ESP_LOGW(TAG, "SD card mount failed on attempt %d", attempt);
            if (attempt <SD_MOUNT_RETRIES){
                vTaskDelay(pdMS_TO_TICKS(SD_RETRY_DELAY_MS));
            }
        }

        if (ret != ESP_OK){
            ESP_LOGE(TAG, "SD card mount failed after %d attempts", attempt);
        }
    }



    // waveshare_esp32_s3_rgb_lcd_init(); // Initialize the Waveshare ESP32-S3 RGB LCD 
    // // wavesahre_rgb_lcd_bl_on();  //Turn on the screen backlight 
    // // wavesahre_rgb_lcd_bl_off(); //Turn off the screen backlight 
    
    // ESP_LOGI(TAG, "Display LVGL demos");
    // // Lock the mutex due to the LVGL APIs are not thread-safe
    // if (lvgl_port_lock(-1)) {
    //     lv_obj_t *label = lv_label_create(lv_scr_act());   // アクティブ画面にラベルを作成
    //     lv_label_set_text(label, "Hello Tac!");            // 表示するテキストを設定
    //     lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);         // 中央に配置
    //     // lv_demo_stress();
    //     // lv_demo_benchmark();
    //     // lv_demo_music();
    //     // lv_demo_widgets();
    //     // example_lvgl_demo_ui();
    //     // Release the mutex
    //     lvgl_port_unlock();
    // }
}
