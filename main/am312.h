#ifndef AM312_H_
#define AM312_H_

#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ユーザコールバック型
 *  level==true  : センサー出力 High (モーション検出)
 *  level==false : センサー出力 Low  (検出なし)
 *  ISR 内から呼ばれるので短時間で返すこと
 */
typedef void (*am312_isr_cb_t)(bool level, void *arg);

/** 初期化用パラメータ */
typedef struct {
    gpio_num_t io_num;   /*!< AM312 の OUT を接続した GPIO */
    bool       inverted; /*!< true: Low=検出, High=非検出 (通常 false) */
} am312_config_t;

/* ---------- API ---------- */

/** 初期化 & (任意で) 割り込みコールバック登録 */
esp_err_t am312_init(const am312_config_t *cfg,
                     am312_isr_cb_t cb, void *cb_arg);

/** 現在レベルを即時取得 (true=モーション) */
bool am312_read(void);

/** GPIO / ISR を開放 */
void am312_deinit(void);

#ifdef __cplusplus
}
#endif
#endif /* AM312_H_ */
