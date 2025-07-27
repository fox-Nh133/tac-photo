#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
    #endif

    esp_err_t i2c_bus_acquire(void);   // acquire i2c driver（install and ref++）
    void      i2c_bus_release(void);   // ref--、delete when ref = 0

    #ifdef __cplusplus
}
#endif
