#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

/* ================= CONFIG ================= */

#define LED_GPIO GPIO_NUM_2
static const char *TAG = "FAST_APP";

/* ========================================== */

void app_main(void)
{
    nvs_flash_init();
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "OTA app running (FAST BLINK)");

    while (1) {
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(200));   // FAST blink
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
