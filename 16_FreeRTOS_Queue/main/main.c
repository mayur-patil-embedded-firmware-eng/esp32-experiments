#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// static const char *TAG = "FREERTOS_DEMO";

TaskHandle_t notify_task;

void sender_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        xTaskNotifyGive(notify_task);
    }
}

void receiver_task(void *arg)
{
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI("NOTIFY", "Notification received");
    }
}

void app_main(void)
{
    xTaskCreate(receiver_task, "receiver", 2048, NULL, 5, &notify_task);
    xTaskCreate(sender_task,   "sender",   2048, NULL, 5, NULL);
}
