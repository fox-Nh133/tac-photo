#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mount SD card and initialize storage
 * @return ESP_OK on success, error code on failure
 */
esp_err_t storage_mount_sdcard(void);

/**
 * @brief Safely unmount SD card
 */
esp_err_t storage_unmount_sdcard(void);

/**
 * @brief Check if SD card is currently mounted
 * @return true if mounted, false otherwise
 */
bool storage_is_sdcard_mounted(void);

/**
 * @brief Get SD card capacity information
 * @param total_bytes Total capacity in bytes
 * @param used_bytes Used capacity in bytes
 * @return ESP_OK on success, error code on failure
 */
esp_err_t storage_get_card_info(uint64_t *total_bytes, uint64_t *used_bytes);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_MANAGER_H