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

    if(evt.ok) {
        size_t img_size = 0;
        void *buf = NULL;
        FILE *f = fopen("/sdcard/sample.jpg", "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            img_size = ftell(f);
            fseek(f, 0, SEEK_SET);

            buf = heap_caps_malloc(img_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if(buf && fread(buf, 1, img_size, f) == img_size) {
                ESP_LOGI(TAG, "Read %zu bytes OK", img_size);
            } else {
                ESP_LOGE(TAG, "Read failed at byte %zu", ftell(f));
            }
            fclose(f);
        }

    }

    xQueueSend(ui_evt_q, &evt, portMAX_DELAY);
    vTaskDelete(NULL);
}

static void show_img_cb(lv_timer_t * timer)
{
    const char *path = (const char *)timer->user_data;

    esp_err_t ret = wavesahre_rgb_lcd_bl_off();
    ESP_LOGI(TAG, "wavesahre_rgb_lcd_bl_off() returned %d", ret);

    vTaskDelay(pdMS_TO_TICKS(SD_RETRY_DELAY_MS));

    size_t img_size = 0;
    void *buf = NULL;
    FILE *f = fopen("/sdcard/sample.jpg", "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        img_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        buf = heap_caps_malloc(img_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if(buf && fread(buf, 1, img_size, f) == img_size) {
            ESP_LOGI(TAG, "Read %zu bytes OK", img_size);
        } else {
            ESP_LOGE(TAG, "Read failed at byte %zu", ftell(f));
        }
        fclose(f);
    }

    lv_obj_t * img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, path);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 60);

    ret = wavesahre_rgb_lcd_bl_on();
    ESP_LOGI(TAG, "wavesahre_rgb_lcd_bl_on() returned %d", ret);

    lv_timer_del(timer);
}


void app_main()
{
    /* ── 1.  LCD を先に初期化 ───────────────────────────── */
    esp_err_t ret = waveshare_esp32_s3_rgb_lcd_init();
    ESP_LOGI(TAG, "waveshare_esp32_s3_rgb_lcd_init() returned %d", ret);
    ret = wavesahre_rgb_lcd_bl_on();
    ESP_LOGI(TAG, "wavesahre_rgb_lcd_bl_on() returned %d", ret);

    /* ── 2.  UI 用イベントキューを生成 ─────────────────── */
    ui_evt_q = xQueueCreate(1, sizeof(sd_evt_t));
    if (!ui_evt_q) {
        ESP_LOGE(TAG, "Failed to create UI event queue!");
        abort();                          // ここは致命的
    }

    /* ── 3.  SD マウントタスクを起動（コア 0）────────────── */
    vTaskDelay(pdMS_TO_TICKS(SD_RETRY_DELAY_MS));          // LCD 起動安定待ち
    xTaskCreatePinnedToCore(sd_mount_task, "sd_mount", 4096,
                            NULL, 3, NULL, 0);

    /* ── 4.  結果を待機 ──────────────────────────────── */
    sd_evt_t sd_evt;
    if (xQueueReceive(ui_evt_q, &sd_evt, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to receive SD mount result!");
        abort();
    }
    ESP_LOGI(TAG, "SD mount task completed: %s", sd_evt.msg);

    /* ── 5.  UI 描画（例：ステータス／画像）────────────── */
    if (lvgl_port_lock(-1)) {
        /* ラベルでステータス表示 */
        lv_obj_t *lbl = lv_label_create(lv_scr_act());
        lv_label_set_text(lbl, sd_evt.msg);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -20);

        /* マウント成功なら 2 s 後に画像を表示 */
        if (sd_evt.ok) {
            lv_timer_t *t = lv_timer_create(show_img_cb, 2000,
                                            (void *)"S:/sample.jpg");
            lv_timer_set_repeat_count(t, 1);
        }
        lvgl_port_unlock();
    }

    vQueueDelete(ui_evt_q);
    ui_evt_q = NULL;
}
