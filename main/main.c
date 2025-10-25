/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include "core/lv_obj_pos.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#include "misc/lv_timer.h"
#include "waveshare_rgb_lcd_port.h"
#include "lvgl_port.h"
#include "storage_manager.h"
#include "widgets/lv_img.h"
#include "lvgl.h"
#include "esp_heap_caps.h"

#define TAG "APP"

#define SD_MOUNT_RETRIES      3
#define SD_RETRY_DELAY_MS     1000

#define SLIDE_DIR             "/sdcard/slides"
#define SLIDE_INTERVAL_MS     10000          // 切替間隔(ms)
#define MAX_IMAGES            64            // 読み込む最大枚数
#define TOTAL_PRELOAD_LIMIT   (16 * 1024 * 1024) // 先読み合計上限(16MB)

typedef struct {
    bool ok;
    char msg[48];
} sd_evt_t;

static QueueHandle_t ui_evt_q = NULL;
static lv_obj_t *img_obj = NULL;

typedef struct {
    uint8_t     *buf;   // 画像のRAWバイト(JPG/PNG)
    size_t       size;  // バイト数
    lv_img_dsc_t dsc;   // LVGLに渡す記述子
    char         name[64]; // ログ用ファイル名(一部のみ)
} image_t;

static image_t g_images[MAX_IMAGES];
static size_t  g_image_count = 0;
static size_t  g_current = 0;

/* 拡張子判定 */
static bool has_image_ext(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    char ext[8] = {0};
    strncpy(ext, dot + 1, sizeof(ext) - 1);
    for (char *p = ext; *p; ++p) *p = (char)tolower((unsigned char)*p);
    return strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0 || strcmp(ext, "png") == 0;
}

/* 1枚読み込み（PSRAM） */
static bool load_file_to_psram(const char *path, image_t *out) {
    memset(out, 0, sizeof(*out));

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "fopen failed: %s", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        ESP_LOGE(TAG, "invalid size: %s", path);
        return false;
    }

    out->buf = (uint8_t *)heap_caps_malloc((size_t)sz + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!out->buf) {
        fclose(f);
        ESP_LOGE(TAG, "malloc failed for %ld bytes: %s", sz, path);
        return false;
    }

    size_t rd = fread(out->buf, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) {
        ESP_LOGE(TAG, "fread short %zu/%ld: %s", rd, sz, path);
        heap_caps_free(out->buf);
        memset(out, 0, sizeof(*out));
        return false;
    }

    out->buf[sz] = 0;       // 安全のため終端
    out->size = (size_t)sz;

    memset(&out->dsc, 0, sizeof(out->dsc));
    out->dsc.header.always_zero = 0;
    out->dsc.header.cf = LV_IMG_CF_RAW; // JPG/PNGの生データ
    out->dsc.header.w = 0;              // 不明でも可（デコーダが解釈）
    out->dsc.header.h = 0;
    out->dsc.data = out->buf;
    out->dsc.data_size = out->size;

    // ログ用名
    const char *slash = strrchr(path, '/');
    snprintf(out->name, sizeof(out->name), "%s", slash ? slash + 1 : path);

    ESP_LOGI(TAG, "Preloaded %s (%u bytes)", out->name, (unsigned)out->size);
    return true;
}

/* 画像を列挙して先読み（合計上限あり） */
static void preload_all_images(void) {
    DIR *dir = opendir(SLIDE_DIR);
    if (!dir) {
        ESP_LOGE(TAG, "opendir failed: %s", SLIDE_DIR);
        return;
    }

    struct dirent *ent;
    size_t total = 0;
    g_image_count = 0;

    while ((ent = readdir(dir)) && g_image_count < MAX_IMAGES) {
        if (ent->d_type == DT_DIR) continue;
        if (!has_image_ext(ent->d_name)) continue;

        // フルパスを動的に構築（切り詰め警告回避）
        size_t need = strlen(SLIDE_DIR) + 1 /*'/'*/ + strlen(ent->d_name) + 1 /*'\0'*/;
        char *full = (char *)heap_caps_malloc(need, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!full) {
            ESP_LOGE(TAG, "malloc failed for full path need=%u", (unsigned)need);
            continue;
        }
        int n = snprintf(full, need, "%s/%s", SLIDE_DIR, ent->d_name);
        if (n < 0 || (size_t)n >= need) {
            ESP_LOGE(TAG, "snprintf truncated? need=%u n=%d", (unsigned)need, n);
            heap_caps_free(full);
            continue;
        }

        struct stat st;
        if (stat(full, &st) != 0 || st.st_size <= 0) {
            heap_caps_free(full);
            continue;
        }
        if (total + (size_t)st.st_size > TOTAL_PRELOAD_LIMIT) {
            ESP_LOGW(TAG, "Preload limit reached (%u/%u). Stop.",
                     (unsigned)total, (unsigned)TOTAL_PRELOAD_LIMIT);
            heap_caps_free(full);
            break;
        }

        if (load_file_to_psram(full, &g_images[g_image_count])) {
            total += (size_t)st.st_size;
            g_image_count++;
        }
        heap_caps_free(full);
    }

    closedir(dir);
    ESP_LOGI(TAG, "Preloaded %u images, total=%u bytes",
             (unsigned)g_image_count, (unsigned)total);
}

/* SDマウントして先読み（LCD起動前に完了させる） */
static void sd_mount_task(void *arg) {
    sd_evt_t evt = { .ok = false };

    for (int attempt = 1; attempt <= SD_MOUNT_RETRIES; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(SD_RETRY_DELAY_MS));
        esp_err_t ret = storage_mount_sdcard();
        if (ret == ESP_OK) {
            preload_all_images();
            if (g_image_count > 0) {
                evt.ok = true;
                snprintf(evt.msg, sizeof(evt.msg), "SD mounted (%d)", attempt);
            } else {
                snprintf(evt.msg, sizeof(evt.msg), "No images");
            }
            break;
        } else {
            ESP_LOGE(TAG, "SD mount failed (%s)", esp_err_to_name(ret));
        }
    }

    if (!evt.ok && g_image_count == 0) {
        snprintf(evt.msg, sizeof(evt.msg), "SD mount failed");
    }

    xQueueSend(ui_evt_q, &evt, portMAX_DELAY);
    vTaskDelete(NULL);
}

/* 表示ロジック（LVGLロック中で呼ぶ） */
static void show_image_locked(size_t idx) {
    if (idx >= g_image_count) return;

    if (!img_obj) {
        img_obj = lv_img_create(lv_scr_act());
        lv_obj_align(img_obj, LV_ALIGN_CENTER, 0, 0);
    }
    lv_img_set_src(img_obj, &g_images[idx].dsc);
    ESP_LOGI(TAG, "Shown: %s (%u bytes)", g_images[idx].name, (unsigned)g_images[idx].size);
}

/* タイマーコールバック */
static void slide_timer_cb(lv_timer_t *t) {
    (void)t;
    if (g_image_count == 0) return;

    if (lvgl_port_lock(-1)) {
        show_image_locked(g_current);
        g_current = (g_current + 1) % g_image_count;
        lvgl_port_unlock();
    }
}

/* メイン */
void app_main(void) {
    ESP_LOGI(TAG, "Mounting SD card");

    sd_evt_t sd_evt;
    ui_evt_q = xQueueCreate(1, sizeof(sd_evt_t));
    if (!ui_evt_q) {
        ESP_LOGE(TAG, "Failed to create queue");
        while (1) { vTaskDelay(1); }
    }

    xTaskCreatePinnedToCore(sd_mount_task, "sd_mount", 8192, NULL, 3, NULL, 0);

    ESP_LOGI(TAG, "Waiting for SD mount...");
    xQueueReceive(ui_evt_q, &sd_evt, portMAX_DELAY);
    ESP_LOGI(TAG, "SD mount done: %s", sd_evt.msg);

    // LCD 初期化（以後はSDに触らないため競合リスクなし）
    esp_err_t ret = waveshare_esp32_s3_rgb_lcd_init();
    ESP_LOGI(TAG, "LCD init = %d", ret);
    wavesahre_rgb_lcd_bl_on();

    if (lvgl_port_lock(-1)) {
        if (sd_evt.ok && g_image_count > 0) {
            // 初回表示 + タイマー開始
            show_image_locked(0);
            lv_timer_t *timer = lv_timer_create(slide_timer_cb, SLIDE_INTERVAL_MS, NULL);
            lv_timer_set_repeat_count(timer, -1);
        } else {
            lv_obj_t *lbl = lv_label_create(lv_scr_act());
            lv_label_set_text(lbl, "No images found");
            lv_obj_center(lbl);
        }
        lvgl_port_unlock();
    }

    vQueueDelete(ui_evt_q);
    ui_evt_q = NULL;
}
