#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/queue.h"

QueueHandle_t queue;

void producer(void *arg)
{
    int value = 0;
    while (1) {
        xQueueSend(queue, &value, portMAX_DELAY);
        value++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void consumer(void *arg)
{
    int rx;
    while (1) {
        xQueueReceive(queue, &rx, portMAX_DELAY);
        ESP_LOGI("QUEUE", "Received: %d", rx);
    }
}

void app_main(void)
{
    queue = xQueueCreate(5, sizeof(int));

    xTaskCreate(producer, "producer", 2048, NULL, 5, NULL);
    xTaskCreate(consumer, "consumer", 2048, NULL, 5, NULL);
}
