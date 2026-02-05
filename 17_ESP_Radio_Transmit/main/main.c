#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "esp_now.h"

/* ================= CONFIG ================= */
static const char *TAG = "ESP_NOW_TX";

/* Receiver ESP32 MAC address */
uint8_t receiver_mac[6] = {0x60, 0x01, 0x94, 0x5E, 0x2E, 0xE3};

/* Example payload */
typedef struct {
    uint32_t counter;
    float temperature;
} payload_t;

/* ========================================== */

/* ESP-NOW send callback */
void espnow_send_cb(const wifi_tx_info_t *tx_info,
                    esp_now_send_status_t status)
{
    ESP_LOGI("ESP_NOW", "Send status: %s",
             status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");
}


/* WiFi init (required for ESP-NOW) */
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialized");
}

/* ESP-NOW init */
static void espnow_init(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, receiver_mac, 6);
    peer.channel = 0;     // same channel
    peer.encrypt = false;

    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_LOGI(TAG, "ESP-NOW initialized");
}

/* Sender task */
void espnow_tx_task(void *arg)
{
    payload_t data = {0};

    while (1) {
        data.counter++;
        data.temperature = 25.5 + (data.counter % 10);

        esp_err_t ret = esp_now_send(
            receiver_mac,
            (uint8_t *)&data,
            sizeof(data)
        );

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Packet sent: count=%lu temp=%.2f",
                     data.counter, data.temperature);
        } else {
            ESP_LOGE(TAG, "Send error: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init();
    espnow_init();

    xTaskCreate(
        espnow_tx_task,
        "espnow_tx_task",
        4096,
        NULL,
        5,
        NULL
    );
}
