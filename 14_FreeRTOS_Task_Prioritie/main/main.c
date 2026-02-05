#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// static const char *TAG = "FREERTOS_DEMO";

void high_task(void *arg)
{
    while (1) {
        ESP_LOGI("HIGH", "High priority task");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void low_task(void *arg)
{
    while (1) {
        ESP_LOGI("LOW", "Low priority task");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    xTaskCreate(high_task, "high", 2048, NULL, 10, NULL);
    xTaskCreate(low_task,  "low",  2048, NULL, 2,  NULL);
}
