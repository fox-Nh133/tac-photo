/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "core/lv_obj_pos.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#include "misc/lv_timer.h"
#include "waveshare_rgb_lcd_port.h"

#include "lvgl_port.h"
#include "portmacro.h"
#include "storage_manager.h"


# define SD_MOUNT_RETRIES 3
# define SD_RETRY_DELAY_MS 1000

typedef struct {
    bool ok;
    char msg[48];
} sd_evt_t;

static QueueHandle_t ui_evt_q; // global

static const char *sample_jpeg_path = "/sdcard/sample.jpg"; //global

static void sd_mount_task(void *arg)
{
    sd_evt_t evt = { .ok = false};
    esp_err_t ret = storage_mount_sdcard();
    const char *jpeg_path = sample_jpeg_path;

    for (int attempt = 1; attempt <= SD_MOUNT_RETRIES; ++attempt){
        ret = storage_mount_sdcard();
        if (ret == ESP_OK) {
            evt.ok = true;
            snprintf(evt.msg, sizeof(evt.msg),
                     "SD mounted on attempt %d", attempt);
            // check jpeg file existence
            FILE *f = fopen(jpeg_path, "rb");
            if (!f) {
                snprintf(evt.msg, sizeof(evt.msg),
                         "JPEG file not fount %s", jpeg_path);
                ESP_LOGW(TAG, "JPEG file not found: %s", jpeg_path);
                return;
            }
            fclose(f);
            snprintf(evt.msg, sizeof(evt.msg),
                     "Found JPEG file %s", jpeg_path);
            ESP_LOGI(TAG, "Found JPEG file: %s", jpeg_path);

            break;
        }
        else {
            ESP_LOGE(TAG, "SD mount failed (%s)", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(SD_RETRY_DELAY_MS));
        }
    }

    if (!evt.ok){
        snprintf(evt.msg, sizeof(evt.msg),
                 "SD mount failed after %d tries", SD_MOUNT_RETRIES);
        ESP_LOGE(TAG, "SD card mount failed after %d attempts", SD_MOUNT_RETRIES);
    }

    xQueueSend(ui_evt_q, &evt, portMAX_DELAY);
    vTaskDelete(NULL);
}

// static void show_img_cb(lv_timer_t * timer)
// {
//     const char *path = (const char *)timer->user_data;
//     lv_obj_t * img = lv_img_create(lv_scr_act());
//     lv_img_set_src(img, path);
//     lv_obj_align(img, LV_ALIGN_CENTER, 0, 60);
//     lv_timer_del(timer);
// }


void app_main()
{
    ESP_LOGI(TAG, "Mounting SD card at first");

    sd_evt_t sd_result_evt;  // the result of sd mounting

    // create UI event queue
    ui_evt_q = xQueueCreate(1, sizeof(sd_evt_t));
    if (ui_evt_q == NULL){
        ESP_LOGE(TAG, "Failed to create UI event queue!");
        while(1) { vTaskDelay(1); }  // stop process as fatal error
    } else {
        ESP_LOGI(TAG, "UI event queue created successfully. Handle: %p", ui_evt_q);
    }

    // activate SD Mount Task
    vTaskDelay(pdMS_TO_TICKS(SD_RETRY_DELAY_MS));
    xTaskCreatePinnedToCore(sd_mount_task, "sd_mount", 4096,
                            NULL, 3, NULL, 0);  // core 0

    // wait until sd mount task completion
    ESP_LOGI(TAG, "Waiting for SD mount task to complete...");
    if (xQueueReceive(ui_evt_q, &sd_result_evt, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to receive SD mount result from queue!");
        while(1) { vTaskDelay(1); } // if this happens, error may happen in sending queue process
    }

    ESP_LOGI(TAG, "SD mount task completed. Result: %s", sd_result_evt.msg);

    // initilalize LCD
    esp_err_t ret = waveshare_esp32_s3_rgb_lcd_init();
    ESP_LOGI(TAG, "waveshare_esp32_s3_rgb_lcd_init() returned %d", ret);
    ret = wavesahre_rgb_lcd_bl_on();
    ESP_LOGI(TAG, "wavesahre_rgb_lcd_bl_on() returned %d", ret);

    // initialize lvgl_port
    if (lvgl_port_lock(-1)){
        if(sd_result_evt.ok) {
            ESP_LOGI(TAG, "creating image object");
            lv_obj_t *img = lv_img_create(lv_scr_act());
            lv_img_set_src(img, "S:/sample.jpg");
            lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
            lvgl_port_unlock();
        }

        lvgl_port_unlock();
    }

    // // set UI
    // if (lvgl_port_lock(-1)) {
    //     lv_obj_t *label = lv_label_create(lv_scr_act());   // create label in active screen
    //     lv_label_set_text(label, "Hello Tac!");            // set display text
    //     lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);         // locate center
    //
    //     lv_obj_t *status_lbl;
    //
    //     status_lbl = lv_label_create(lv_scr_act());
    //     lv_label_set_text(status_lbl, sd_result_evt.msg); // display received message
    //     lv_color_t c = sd_result_evt.ok ? lv_palette_main(LV_PALETTE_GREEN): lv_palette_main(LV_PALETTE_RED);
    //     lv_obj_set_style_text_color(status_lbl, c, 0); // set correspondin color
    //     lv_obj_align(status_lbl,LV_ALIGN_CENTER, 0, +20);
    //
    //     if(sd_result_evt.ok) {
    //         lv_timer_t * t = lv_timer_create(show_img_cb, 2000, (void *)sample_jpeg_path);
    //         lv_timer_set_repeat_count(t, 1);
    //     }
    //
    //
    //     lvgl_port_unlock();
    // }

    vQueueDelete(ui_evt_q);
    ui_evt_q = NULL;
}
