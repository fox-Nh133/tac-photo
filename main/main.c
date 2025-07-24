/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>

#include "storage_manager.h"

#include "waveshare_rgb_lcd_port.h"

void app_main()
{
    // wait a while after boot
    vTaskDelay(pdMS_TO_TICKS(1000));
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
