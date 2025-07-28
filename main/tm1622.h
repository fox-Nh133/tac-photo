#ifndef TM1622_H
#define TM1622_H

/**
 * @file tm1622.h
 * @brief Minimal driver for TM1622/TM1623/HT1622 16‑segment LCD controller for ESP‑IDF (ESP32‑S3)
 *
 * This header exposes a very small, bit‑banged interface that is easy to
 * integrate into any ESP‑IDF project.  It is based on the Arduino reference
 * driver found in MartyMacGyver/LCD_HT1622_16SegLcd but rewritten for the
 * ESP32‑S3 GPIO and timing primitives.
 *
 * ────────────────────────────────────────────────────────────────────────────────
 *  Wiring (4‑wire serial, RD optional):
 *      TM1622  ───►  ESP32‑S3 GPIO
 *      ───────────────────────────────────────
 *        CS    ───►  pin_cs   (active‑low)
 *        WR    ───►  pin_wr   (rising‑edge latch)
 *        DATA  ───►  pin_data (MOSI in WRITE mode)
 *        RD    (not used – tie high)
 *        VDD   ───►  5 V (or 3.3 V module‑dependent)
 *        GND   ───►  GND
 *        OSCI  ───►  NC (internal RC osc)
 *
 *  Typical init command sequence:
 *      SYS EN  (0x01)
 *      LCD ON  (0x03)
 *      RC 32K  (0x18)
 *      TONEOFF (0x08)
 *      IRQ DIS (0x90)
 *      F1      (0xA0)  // 1 Hz time‑base (optional)
 *
 *  One WRITE transaction moves 4 bits into one address.  The start address is
 *  6 bits wide (0‥0x3F).  For a 10‑digit ×16‑segment glass you will typically
 *  use 0x00‥0x27 inclusive.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 *  Public defines – command byte helpers
 * ========================================================================= */
#define TM1622_CMD_SYS_EN   0x01u  /*!< Enable system oscillator        */
#define TM1622_CMD_SYS_DIS  0x00u  /*!< Disable system oscillator        */
#define TM1622_CMD_LCD_ON   0x03u  /*!< Turn on LCD bias generator       */
#define TM1622_CMD_LCD_OFF  0x02u  /*!< Turn off LCD bias generator      */
#define TM1622_CMD_RC_32K   0x18u  /*!< Use internal 32 kHz RC clock     */
#define TM1622_CMD_EXT_32K  0x1Cu  /*!< Use external 32 kHz clock        */
#define TM1622_CMD_TONE_OFF 0x08u  /*!< Disable buzzer output            */
#define TM1622_CMD_IRQ_DIS  0x90u  /*!< Disable IRQ pin                  */
#define TM1622_CMD_F1       0xA0u  /*!< 1 Hz time‑base (WDT 4 s)         */

/* =========================================================================
 *  Handle / configuration structure
 * ========================================================================= */
typedef struct {
    gpio_num_t pin_cs;    /*!< Chip‑select (active‑low) */
    gpio_num_t pin_wr;    /*!< Write clock             */
    gpio_num_t pin_data;  /*!< Bidirectional data pin  */
} tm1622_t;

/* =========================================================================
 *  High‑level API
 * ========================================================================= */
/**
 * @brief  Configure GPIOs and send the default initialisation sequence.
 *         You may call additional tm1622_send_command() afterwards to tweak
 *         settings (time‑base, buzzer, etc.).
 */
esp_err_t tm1622_init(const tm1622_t *dev);

/** @brief Send one 8‑bit command in COMMAND mode. */
void tm1622_send_command(const tm1622_t *dev, uint8_t cmd);

/**
 * @brief Write 4 bits of display RAM.
 * @param addr   6‑bit start address (0‥63) ‑ lower 2 bits select nibble inside
 *               the 16‑bit row (Digit×4 + nibble‑index).
 * @param data4  Lower 4 bits contain the pixel pattern.
 */
void tm1622_write4(const tm1622_t *dev, uint8_t addr, uint8_t data4);

/**
 * @brief Burst‑write consecutive 4‑bit values starting at @p start_addr.
 */
void tm1622_write_bulk(const tm1622_t *dev, uint8_t start_addr,
                       const uint8_t *data4, size_t len);

/** @brief Set all RAM bits to on/off (all‑segments test). */
void tm1622_set_all(const tm1622_t *dev, bool on);

/** @brief Clear entire display RAM (all pixels off). */
void tm1622_clear(const tm1622_t *dev);

/**
 * @brief  Draw a single ASCII character (subset) at digit index.
 *         Index 0 = left‑most digit on the glass.
 */
esp_err_t tm1622_putc(const tm1622_t *dev, uint8_t digit, char c);

/** @brief  Print a zero‑terminated string starting at digit 0. */
esp_err_t tm1622_puts(const tm1622_t *dev, const char *s);

#ifdef __cplusplus
}
#endif

#endif /* TM1622_H */
