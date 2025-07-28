#include "am312.h"
#include "esp_log.h"

static const char *TAG = "am312";

static am312_config_t      s_cfg;
static am312_isr_cb_t      s_user_cb  = NULL;
static void               *s_user_arg = NULL;
static bool                s_inited   = false;

/* ISR ハンドラ */
static void IRAM_ATTR am312_isr_handler(void *arg)
{
    bool level = gpio_get_level(s_cfg.io_num);
    if (s_cfg.inverted) level = !level;
    if (s_user_cb) s_user_cb(level, s_user_arg);
}

esp_err_t am312_init(const am312_config_t *cfg,
                     am312_isr_cb_t cb, void *cb_arg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    if (s_inited) return ESP_OK;

    s_cfg = *cfg;
    s_user_cb  = cb;
    s_user_arg = cb_arg;

    /* GPIO 設定 */
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << s_cfg.io_num,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = cb ? GPIO_INTR_ANYEDGE : GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    /* ISR サービス登録（コールバック指定時のみ） */
    if (cb) {
        ESP_ERROR_CHECK(gpio_install_isr_service(0));
        ESP_ERROR_CHECK(gpio_isr_handler_add(s_cfg.io_num,
                                             am312_isr_handler, NULL));
    }

    s_inited = true;
    ESP_LOGI(TAG, "AM312 initialised on GPIO%d (inverted=%d)",
             s_cfg.io_num, s_cfg.inverted);
    return ESP_OK;
}

bool am312_read(void)
{
    if (!s_inited) return false;
    bool level = gpio_get_level(s_cfg.io_num);
    return s_cfg.inverted ? !level : level;
}

void am312_deinit(void)
{
    if (!s_inited) return;
    if (s_user_cb) {
        gpio_isr_handler_remove(s_cfg.io_num);
        gpio_uninstall_isr_service();
    }
    gpio_reset_pin(s_cfg.io_num);
    s_inited = false;
}
