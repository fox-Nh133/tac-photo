/*
 * tm1622.c – ESP‑IDF bit‑banged driver for TM1622 / HT1622 4‑wire LCD controller
 *
 * This implementation favours clarity over raw speed.  With a 10‑digit display
 * you will typically push < 160 nibbles per full refresh, which easily fits
 * into real‑time constraints even with microsecond delays.
 */

#include "tm1622.h"
#include "esp_rom_sys.h"   /* esp_rom_delay_us() */
#include "driver/gpio.h"
#include <string.h>

/* ≈400 ns WR high‑time @80 MHz core – extra margin added */
#define WR_PULSE_US 1U

/* ------------------------------------------------------------------------- */
static inline void gpio_out(gpio_num_t pin, int lvl)
{
    gpio_set_level(pin, lvl);
}

static inline void wr_pulse(const tm1622_t *dev, int data_level)
{
    gpio_out(dev->pin_wr, 0);
    gpio_out(dev->pin_data, data_level);
    gpio_out(dev->pin_wr, 1);
    esp_rom_delay_us(WR_PULSE_US);
}

static void send_bits(const tm1622_t *dev, uint32_t value, uint8_t count)
{
    for (int i = count - 1; i >= 0; --i) {
        wr_pulse(dev, (value >> i) & 1U);
    }
}

/* -------------------------------------------------------------------------
 *  Low‑level helpers
 * ------------------------------------------------------------------------- */
static void cs_enable(const tm1622_t *dev)  { gpio_out(dev->pin_cs, 0); }
static void cs_disable(const tm1622_t *dev) { gpio_out(dev->pin_cs, 1); }

/* -------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */
esp_err_t tm1622_init(const tm1622_t *dev)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << dev->pin_cs) |
                        (1ULL << dev->pin_wr) |
                        (1ULL << dev->pin_data),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) return ret;

    /* Idle levels */
    gpio_out(dev->pin_cs,   1);
    gpio_out(dev->pin_wr,   1);
    gpio_out(dev->pin_data, 0);

    /* Send canonical initialisation sequence. */
    tm1622_send_command(dev, TM1622_CMD_SYS_EN);
    tm1622_send_command(dev, TM1622_CMD_LCD_ON);
    tm1622_send_command(dev, TM1622_CMD_RC_32K);
    tm1622_send_command(dev, TM1622_CMD_TONE_OFF);
    tm1622_send_command(dev, TM1622_CMD_IRQ_DIS);
    tm1622_send_command(dev, TM1622_CMD_F1);

    tm1622_clear(dev);
    return ESP_OK;
}

void tm1622_send_command(const tm1622_t *dev, uint8_t cmd)
{
    cs_enable(dev);
    send_bits(dev, 0b100U, 3);      /* COMMAND type */
    send_bits(dev, cmd & 0xFFu, 8);
    cs_disable(dev);
}

void tm1622_write4(const tm1622_t *dev, uint8_t addr, uint8_t data4)
{
    cs_enable(dev);
    send_bits(dev, 0b101U, 3);             /* WRITE type */
    send_bits(dev, addr & 0x3FU, 6);       /* 6‑bit address */
    send_bits(dev, data4 & 0x0FU, 4);      /* 4‑bit payload */
    cs_disable(dev);
}

void tm1622_write_bulk(const tm1622_t *dev, uint8_t start_addr,
                       const uint8_t *data4, size_t len)
{
    cs_enable(dev);
    send_bits(dev, 0b101U, 3);             /* WRITE type */
    send_bits(dev, start_addr & 0x3FU, 6);
    for (size_t i = 0; i < len; ++i) {
        send_bits(dev, data4[i] & 0x0FU, 4);
    }
    cs_disable(dev);
}

void tm1622_set_all(const tm1622_t *dev, bool on)
{
    uint8_t buf[32]; /* 32 × 4bit = 128 nibbles = 256 segments; enough */
    memset(buf, on ? 0x0F : 0x00, sizeof(buf));
    tm1622_write_bulk(dev, 0, buf, sizeof(buf));
}

void tm1622_clear(const tm1622_t *dev)
{
    tm1622_set_all(dev, false);
}

/* -------------------------------------------------------------------------
 *  Simple ASCII –> 16‑segment map (common‑anode active‑high)
 *  Each bit represents: 0=a,1=b,2=c,3=d,4=e,5=f,6=g,7=h,8=i,9=j,10=k,11=l,
 *                       12=m,13=n,14=p,15=u.
 *  Adjust to your glass if segment order differs.
 * ------------------------------------------------------------------------- */
static const uint16_t font_table[128] = {
    ['0'] = 0b0011111111111111, /* all but g */
    ['1'] = 0b0000000000110000, /* b,c           */
    ['2'] = 0b0011110011101111, /* a,b,d,e,g,p   */
    ['3'] = 0b0011110010111111, /* a,b,c,d,g,p   */
    ['4'] = 0b0000001100110011, /* f,g,b,c       */
    ['5'] = 0b0011111100111110, /* a,f,g,c,d,p   */
    ['6'] = 0b0011111111111110, /* a,f,e,d,c,g,p */
    ['7'] = 0b0000000000111111, /* a,b,c         */
    ['8'] = 0b0011111111111111, /* all           */
    ['9'] = 0b0011111100111111, /* a,b,c,d,f,g,p */
    ['A'] = 0b0011001111111111, /* a,b,c,e,f,g   */
    ['B'] = 0b0000111111111100, /* c,d,e,f,g,h,k */
    ['C'] = 0b0011110000001111, /* a,d,e,f       */
    ['D'] = 0b0000110011111100, /* b,c,d,e,g,k   */
    ['E'] = 0b0011111110001111, /* a,d,e,f,g     */
    ['F'] = 0b0011001110001111, /* a,e,f,g       */
    ['-'] = 0b0000000010000000, /* g */
    [' '] = 0x0000
};

static esp_err_t raw_put_nibble(const tm1622_t *dev, uint8_t digit, uint8_t nib_idx, uint8_t data4)
{
    if (digit >= 10 || nib_idx >= 4) return ESP_ERR_INVALID_ARG;
    uint8_t addr = digit * 4 + nib_idx;
    tm1622_write4(dev, addr, data4);
    return ESP_OK;
}

esp_err_t tm1622_putc(const tm1622_t *dev, uint8_t digit, char c)
{
    uint16_t pattern = font_table[(uint8_t)c];
    for (uint8_t nib = 0; nib < 4; ++nib) {
        uint8_t data4 = (pattern >> (nib * 4)) & 0x0F;
        esp_err_t ret = raw_put_nibble(dev, digit, nib, data4);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}

esp_err_t tm1622_puts(const tm1622_t *dev, const char *s)
{
    uint8_t idx = 0;
    while (*s && idx < 10) {
        tm1622_putc(dev, idx++, *s++);
    }
    /* Clear remaining digits */
    while (idx < 10) {
        tm1622_putc(dev, idx++, ' ');
    }
    return ESP_OK;
}
