/*************************************************
 * ESP32 + 16x2 I2C LCD (PCF8574)
 * Print "Hello World"
 * ESP-IDF C | Single Source File
 *************************************************/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

/* ================= USER CONFIG ================= */
#define I2C_MASTER_NUM     I2C_NUM_0
#define I2C_MASTER_SDA_IO  21
#define I2C_MASTER_SCL_IO  22
#define I2C_MASTER_FREQ_HZ 100000

#define LCD_I2C_ADDR       0x27   // Try 0x3F if not working

/* ================= LCD DEFINES ================= */
#define LCD_BACKLIGHT  0x08
#define LCD_ENABLE     0x04
#define LCD_RS         0x01

static const char *TAG = "LCD";

/* ================= I2C INIT ================= */
static void i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

/* ================= LOW LEVEL LCD ================= */
static void lcd_i2c_write(uint8_t data)
{
    i2c_master_write_to_device(
        I2C_MASTER_NUM,
        LCD_I2C_ADDR,
        &data,
        1,
        pdMS_TO_TICKS(100)
    );
}

static void lcd_pulse_enable(uint8_t data)
{
    lcd_i2c_write(data | LCD_ENABLE);
    esp_rom_delay_us(1);          // FIXED
    lcd_i2c_write(data & ~LCD_ENABLE);
    esp_rom_delay_us(50);         // FIXED
}

static void lcd_send_nibble(uint8_t nibble, uint8_t mode)
{
    uint8_t data = (nibble << 4) | LCD_BACKLIGHT | mode;
    lcd_i2c_write(data);
    lcd_pulse_enable(data);
}

static void lcd_send_byte(uint8_t byte, uint8_t mode)
{
    lcd_send_nibble(byte >> 4, mode);
    lcd_send_nibble(byte & 0x0F, mode);
}

/* ================= LCD COMMANDS ================= */
static void lcd_cmd(uint8_t cmd)
{
    lcd_send_byte(cmd, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
}

static void lcd_data(uint8_t data)
{
    lcd_send_byte(data, LCD_RS);
}

static void lcd_clear(void)
{
    lcd_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(5));
}

static void lcd_set_cursor(uint8_t col, uint8_t row)
{
    uint8_t row_offsets[] = {0x00, 0x40};
    lcd_cmd(0x80 | (col + row_offsets[row]));
}

/* ================= LCD INIT ================= */
static void lcd_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(50));

    lcd_send_nibble(0x03, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_nibble(0x03, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_nibble(0x03, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_send_nibble(0x02, 0);

    lcd_cmd(0x28); // 4-bit, 2-line
    lcd_cmd(0x0C); // Display ON, cursor OFF
    lcd_cmd(0x06); // Auto increment
    lcd_clear();
}

/* ================= PRINT STRING ================= */
static void lcd_print(const char *str)
{
    while (*str) {
        lcd_data((uint8_t)*str++);
    }
}

/* ================= MAIN APP ================= */
void app_main(void)
{
    ESP_LOGI(TAG, "Initializing I2C");
    i2c_master_init();

    ESP_LOGI(TAG, "Initializing LCD");
    lcd_init();

    lcd_set_cursor(0, 0);
    lcd_print("Hello World");

    lcd_set_cursor(0, 1);
    lcd_print("ESP32 + I2C");

    ESP_LOGI(TAG, "Message displayed");
}
