/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "core/lv_obj_pos.h"
#include "esp_lcd_panel_ops.h"
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
#include "widgets/lv_img.h"


# define SD_MOUNT_RETRIES 3
# define SD_RETRY_DELAY_MS 1000

extern const lv_img_dsc_t landscape_sample_1;

typedef struct {
    bool ok;
    char msg[48];
} sd_evt_t;

static QueueHandle_t ui_evt_q; // global

static const char *jpeg_path = "/sdcard/sample_2.jpg"; //global

static uint8_t *g_jpeg_buf = NULL; //global
static size_t g_jpeg_size = 0;
static lv_img_dsc_t jpeg_raw;

static const char *TAG_IMAGE = "jpeg_decoder";

esp_err_t load_file_to_psram(const char *path,
                             uint8_t **out_buf,
                             size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG_IMAGE, "fopen(%s) failed", path);
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }

    /* +1 は LVGL の src_type 判定用セーフティ（終端） */
    uint8_t *buf = heap_caps_malloc(file_size + 1,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t rd = fread(buf, 1, file_size, f);
    fclose(f);

    if (rd != file_size) {
        ESP_LOGE(TAG_IMAGE, "read short (%zu / %ld)", rd, file_size);
        free(buf);
        return ESP_FAIL;
    }

    buf[file_size] = 0;          /* 終端バイトを入れておくと安心 */
    *out_buf  = buf;
    *out_size = file_size;
    ESP_LOGI(TAG_IMAGE, "JPEG %ld bytes loaded to 0x%p", file_size, buf);
    return ESP_OK;
}

static void sd_mount_task(void *arg)
{
    sd_evt_t evt = { .ok = false};
    esp_err_t ret;

    for (int attempt = 1; attempt <= SD_MOUNT_RETRIES; ++attempt){
        vTaskDelay(pdMS_TO_TICKS(SD_RETRY_DELAY_MS));
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
        }
    }

    if (!evt.ok){
        snprintf(evt.msg, sizeof(evt.msg),
                 "SD mount failed after %d tries", SD_MOUNT_RETRIES);
        ESP_LOGE(TAG, "SD card mount failed after %d attempts", SD_MOUNT_RETRIES);
    }

    if (evt.ok) {
        if (load_file_to_psram(jpeg_path,
            &g_jpeg_buf, &g_jpeg_size) != ESP_OK) {
            ESP_LOGE(TAG, "JPEG load failed");
        }
    }

    // if (evt.ok) {
    //     if (load_file_to_psram("/sdcard/sample.jpg", &g_jpeg_buf, &g_jpeg_size) == ESP_OK) {
    //         evt.ok = true;
    //         strncpy(evt.msg, "JPEG copied to PSRAM", sizeof(evt.msg));
    //     } else {
    //         evt.ok = false;
    //         strncpy(evt.msg, "JPEG copy failed", sizeof(evt.msg));
    //     }
    // }

    // if(evt.ok) {
    //     size_t img_size = 0;
    //     FILE *f = fopen("/sdcard/sample.bmp", "rb");
    //     if (f) {
    //         fseek(f, 0, SEEK_END);
    //         img_size = ftell(f);
    //         fseek(f, 0, SEEK_SET);
    //
    //         g_jpeg_buf = heap_caps_malloc(img_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    //         if(g_jpeg_buf && fread(g_jpeg_buf, 1, img_size, f) == img_size) {
    //             ESP_LOGI(TAG, "Read %zu bytes OK", img_size);
    //         } else {
    //             ESP_LOGE(TAG, "Read failed at byte %zu", ftell(f));
    //         }
    //         fclose(f);
    //     }
    //
    // }

    xQueueSend(ui_evt_q, &evt, portMAX_DELAY);
    vTaskDelete(NULL);
}

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

    // ret = wavesahre_rgb_lcd_bl_on();
    // ESP_LOGI(TAG, "wavesahre_rgb_lcd_bl_on() returned %d", ret);

    esp_lcd_panel_handle_t panel_handle =
    waveshare_rgb_lcd_get_panel();

    // initialize segment lcd
    // tm1622_t lcd = {
    //     .pin_cs = GPIO_NUM_43,
    //     .pin_wr = GPIO_NUM_44,
    //     .pin_data = GPIO_NUM_6,
    // };
    //
    // ESP_ERROR_CHECK( tm1622_init(&lcd) );
    // tm1622_puts(&lcd, "Tac Projcet");・

    // set UI
    if (lvgl_port_lock(-1)) {
        // lv_obj_t *label = lv_label_create(lv_scr_act());   // create label in active screen
        // lv_label_set_text(label, "Hello Tac!");            // set display text
        // lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);         // locate center
        //
        // lv_obj_t *status_lbl;
        //
        // status_lbl = lv_label_create(lv_scr_act());
        // lv_label_set_text(status_lbl, sd_result_evt.msg); // display received message
        // lv_color_t c = sd_result_evt.ok ? lv_palette_main(LV_PALETTE_GREEN): lv_palette_main(LV_PALETTE_RED);
        // lv_obj_set_style_text_color(status_lbl, c, 0); // set correspondin color
        // lv_obj_align(status_lbl,LV_ALIGN_CENTER, 0, +20);

        if(sd_result_evt.ok) {

            jpeg_raw.data       = g_jpeg_buf;
            jpeg_raw.data_size  = g_jpeg_size;
            jpeg_raw.header.cf  = LV_IMG_CF_RAW;   // 圧縮データを LVGL に渡す
            jpeg_raw.header.always_zero = 0;
            jpeg_raw.header.w   = 800;             // 不明なら 0 でも表示可
            jpeg_raw.header.h   = 480;

            // esp_lcd_panel_disp_on_off(panel_handle, false);
            lv_obj_t *img = lv_img_create(lv_scr_act());
            lv_img_set_src(img, &jpeg_raw);
            // esp_lcd_panel_disp_on_off(panel_handle, true);
            lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
            // lv_timer_t * t = lv_timer_create(show_img_cb, 2000, NULL);
            // lv_timer_set_repeat_count(t, 1);
        } else {
            lv_obj_t *label = lv_label_create(lv_scr_act());   // create label in active screen
            lv_label_set_text(label, "Hello Tac!");            // set display text
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);         // locate center

            lv_obj_t *status_lbl;

            status_lbl = lv_label_create(lv_scr_act());
            lv_label_set_text(status_lbl, sd_result_evt.msg); // display received message
            lv_color_t c = lv_palette_main(LV_PALETTE_RED);
            lv_obj_set_style_text_color(status_lbl, c, 0); // set correspondin color
            lv_obj_align(status_lbl,LV_ALIGN_CENTER, 0, +20);

            lv_obj_t *img = lv_img_create(lv_scr_act());
            lv_img_set_src(img, &landscape_sample_1); /* & を付ける！ */
            lv_obj_center(img);

        }
        lvgl_port_unlock();

    }

    ret = wavesahre_rgb_lcd_bl_on();
    ESP_LOGI(TAG, "wavesahre_rgb_lcd_bl_on() returned %d", ret);

    vQueueDelete(ui_evt_q);
    ui_evt_q = NULL;
}
