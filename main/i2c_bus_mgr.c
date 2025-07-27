#include "i2c_bus_mgr.h"
#include "driver/i2c.h"
#include "esp_check.h"

#define TAG "i2c_bus_mgr"
#define BUS I2C_NUM_0          // fix IO8=SDA, IO9=SCL

static int ref_cnt = 0;        // reference counter

esp_err_t i2c_bus_acquire(void)
{
    /* if already installed, just ref++ */
    if (ref_cnt > 0) { ref_cnt++; return ESP_OK; }

    /* install driver only for tha first time */
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 8,
        .scl_io_num = 9,
        .master.clk_speed = 400000
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(BUS, &cfg),  TAG, "");
    esp_err_t ret = i2c_driver_install(BUS, cfg.mode, 0, 0, 0);
    if (ret == ESP_ERR_INVALID_STATE) ret = ESP_OK;   // already installed

    if (ret == ESP_OK) ref_cnt = 1;                   // successfully mounted
    return ret;
}

void i2c_bus_release(void)
{
    if (ref_cnt == 0) return;     // prevent abnormal calls
    if (--ref_cnt == 0) {
        i2c_driver_delete(BUS);   // delete when the last user release it
    }
}
