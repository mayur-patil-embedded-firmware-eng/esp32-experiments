#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "FREERTOS_DEMO";

void task1(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "Task 1 running");
        vTaskDelay(pdMS_TO_TICKS(1000));   // sleep 1 sec
    }
}

void app_main(void)
{
    xTaskCreate(
        task1,            // Task function
        "Task1",           // Name
        2048,              // Stack size
        NULL,              // Parameters
        5,                 // Priority
        NULL               // Task handle
    );
}
