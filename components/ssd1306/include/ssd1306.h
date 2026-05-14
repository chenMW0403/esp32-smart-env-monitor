#pragma once
/**
 * @file ssd1306.h
 * @brief SSD1306 128x64 OLED Display Driver (I2C)
 *
 * Minimal implementation: init, clear, draw string, update.
 * Uses a 128x8 page buffer (1KB) for the full display.
 * Font: built-in 6x8 ASCII font (chars 0x20-0x7F).
 */

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64
#define SSD1306_PAGES   (SSD1306_HEIGHT / 8)  /* 8 pages */

typedef struct {
    i2c_master_dev_handle_t dev;
    uint8_t buffer[SSD1306_WIDTH * SSD1306_PAGES];
} ssd1306_handle_t;

/**
 * @brief Initialize SSD1306 OLED on existing I2C bus
 * @param handle    Output handle
 * @param bus       I2C master bus handle
 * @param addr      I2C address (typically 0x3C or 0x3D)
 * @return ESP_OK on success
 */
esp_err_t ssd1306_init(ssd1306_handle_t *handle,
                       i2c_master_bus_handle_t bus,
                       uint16_t addr);

/**
 * @brief Clear the display buffer (call ssd1306_update to flush)
 */
void ssd1306_clear(ssd1306_handle_t *handle);

/**
 * @brief Draw a single character at pixel position
 * @param x     X pixel (0-127)
 * @param page  Page (0-7, each page = 8 pixel rows)
 * @param c     ASCII character (0x20-0x7F)
 */
void ssd1306_draw_char(ssd1306_handle_t *handle, uint8_t x, uint8_t page, char c);

/**
 * @brief Draw a null-terminated string at pixel position
 * @param x     Starting X pixel
 * @param page  Page number
 * @param str   String to draw
 */
void ssd1306_draw_string(ssd1306_handle_t *handle, uint8_t x, uint8_t page, const char *str);

/**
 * @brief Flush buffer to display
 * @return ESP_OK on success
 */
esp_err_t ssd1306_update(ssd1306_handle_t *handle);

#ifdef __cplusplus
}
#endif
