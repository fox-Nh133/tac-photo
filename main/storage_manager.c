#include "storage_manager.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_check.h"
#include "i2c_bus_mgr.h"

#include <stdint.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/i2c.h"

// I2C configuration
#define I2C_MASTER_SCL_IO CONFIG_STORAGE_I2C_SCL
#define I2C_MASTER_SDA_IO CONFIG_STORAGE_I2C_SDA
#define I2C_MASTER_NUM 0
#define I2C_MASTER_FREQ_HZ 400000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_MASTER_TIMEOUT_MS 1000

// Maximum character size for file operations
#define MAX_FILE_CHAR_SIZE 64

// Mount point for SD card
#define MOUNT_POINT "/sdcard"

// Pin assignments for SD SPI interface
#define PIN_NUM_MISO CONFIG_STORAGE_PIN_MISO
#define PIN_NUM_MOSI CONFIG_STORAGE_PIN_MOSI
#define PIN_NUM_CLK  CONFIG_STORAGE_PIN_CLK
#define PIN_NUM_CS   CONFIG_STORAGE_PIN_CS

static const char *TAG = "storage";
static bool is_mounted = false;

sdmmc_card_t *card;
const char mount_point[] = MOUNT_POINT;

// By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
// For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
// Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
sdmmc_host_t host = SDSPI_HOST_DEFAULT();

esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;

    // Configure I2C parameters
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,                // Set to master mode
        .sda_io_num = I2C_MASTER_SDA_IO,        // Set SDA pin
        .scl_io_num = I2C_MASTER_SCL_IO,        // Set SCL pin
        .sda_pullup_en = GPIO_PULLUP_ENABLE,    // Enable SDA pull-up
        .scl_pullup_en = GPIO_PULLUP_ENABLE,    // Enable SCL pull-up
        .master.clk_speed = I2C_MASTER_FREQ_HZ, // Set I2C clock speed
    };

    esp_err_t err;

    // Apply I2C configuration
    err = i2c_param_config(i2c_master_port, &conf);
    if (err != ESP_OK) {
        ESP_LOGE("i2c", "i2c_param_config failed: %s", esp_err_to_name(err));
        return err;
    }

    // Install I2C driver
    err = i2c_driver_install(i2c_master_port, conf.mode,
                             I2C_MASTER_RX_BUF_DISABLE,
                             I2C_MASTER_TX_BUF_DISABLE, 0);
    if (err != ESP_OK) {
        ESP_LOGE("i2c", "i2c_driver_install failed: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t storage_mount_sdcard()
{
    esp_err_t ret;
    // Initialize I2C
    ESP_RETURN_ON_ERROR(i2c_bus_acquire(), TAG, "I2C bus acquire failed");

    // ret = i2c_master_init();
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "i2c_master_init failed: %s", esp_err_to_name(ret));
    //     return ESP_FAIL;
    // }

    // Control CH422G to pull down the CS pin of the SD
    uint8_t write_buf = 0x01;
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, 0x24, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write to 0x24 failed: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    write_buf = 0x0A;
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write to 0x38 failed: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    // Options for mounting the filesystem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true, // If mount fails, format the card
#else
        .format_if_mount_failed = false, // If mount fails, do not format card
#endif
        .max_files = 5,                   // Maximum number of files
        .allocation_unit_size = 16 * 1024 // Set allocation unit size
    };

    // Initializing SD card
    ESP_LOGW(TAG, "Initializing SD card");

    // Configure SPI bus for SD card configuration
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI, // Set MOSI pin
        .miso_io_num = PIN_NUM_MISO, // Set MISO pin
        .sclk_io_num = PIN_NUM_CLK,  // Set SCLK pin
        .quadwp_io_num = -1,         // Not used
        .quadhd_io_num = -1,         // Not used
        .max_transfer_sz = 4000,     // Maximum transfer size
    };

    // host.max_freq_khz = 10000; // Set maximum frequency to 10MHz

    // Initialize SPI bus
    // at first, free the SPI bus in case it was already initialized
    spi_bus_free(host.slot);

    ESP_GOTO_ON_ERROR(spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA),
                      cleanup, TAG, "spi_bus_initialize failed");

    // Configure SD card slot
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS; // Set CS pin
    slot_config.host_id = host.slot;  // Set host ID

    // Mounting filesystem
    ESP_LOGW(TAG, "Mounting filesystem");
    ESP_GOTO_ON_ERROR(esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card),
                      cleanup, TAG, "esp_vfs_fat_sdspi_mount failed");


    // Filesystem mounted
    ESP_LOGI(TAG, "Filesystem mounted");
    is_mounted = true;
    return ESP_OK;

cleanup:
    i2c_bus_release();
    if (ret != ESP_OK){
        spi_bus_free(host.slot); // release if failed in initialization
    }
    return ret;
}

esp_err_t storage_unmount_sdcard(void){
    if (!is_mounted) return ESP_ERR_INVALID_STATE;

    // Unmount the filesystem
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    is_mounted = false;
    ESP_LOGI(TAG, "SD card unmounted.");

    // Deinitialize SPI bus
    spi_bus_free(host.slot);
    return ESP_OK;
}

// check if SD card is mounted
bool storage_is_sdcard_mounted(void)
{
    return is_mounted;
}

// get SD card capacity information (dummy implementation)
esp_err_t storage_get_card_info(uint64_t *total_bytes, uint64_t *used_bytes)
{
    if (!is_mounted) return ESP_ERR_INVALID_STATE;

    // NOTE: add actual implementation to retrieve SD card info
    // For now, we return dummy values
    *total_bytes = 0;
    *used_bytes = 0;
    return ESP_OK;
}
