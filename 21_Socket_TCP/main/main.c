#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "driver/gpio.h"

/* ================= USER CONFIG ================= */
#define WIFI_SSID      "TP-Link_C782"
#define WIFI_PASS      "Nayan@9173721024"

#define LED_GPIO       GPIO_NUM_2
#define SERVER_PORT    3333

static const char *TAG = "SOCKET_LED";

/* ================= WIFI EVENT ================= */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START)
        esp_wifi_connect();

    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        esp_wifi_connect();
}

/* ================= WIFI INIT ================= */
void wifi_init(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                               &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

/* ================= SOCKET SERVER TASK ================= */
void socket_server_task(void *pvParameters)
{
    char rx_buffer[128];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket create failed");
        vTaskDelete(NULL);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(sock, 1);

    ESP_LOGI(TAG, "Socket server listening on port %d", SERVER_PORT);

    while (1) {
        int client_sock = accept(sock,
                                 (struct sockaddr *)&client_addr,
                                 &addr_len);

        ESP_LOGI(TAG, "Client connected");

        while (1) {
            int len = recv(client_sock, rx_buffer,
                           sizeof(rx_buffer) - 1, 0);

            if (len <= 0)
                break;

            rx_buffer[len] = 0;   // null terminate
            ESP_LOGI(TAG, "Received: %s", rx_buffer);

            if (strstr(rx_buffer, "ON")) {
                gpio_set_level(LED_GPIO, 1);
                send(client_sock, "LED ON\n", 7, 0);
            }
            else if (strstr(rx_buffer, "OFF")) {
                gpio_set_level(LED_GPIO, 0);
                send(client_sock, "LED OFF\n", 8, 0);
            }
        }

        close(client_sock);
        ESP_LOGI(TAG, "Client disconnected");
    }
}

/* ================= MAIN ================= */
void app_main(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    wifi_init();

    vTaskDelay(5000 / portTICK_PERIOD_MS); // wait for WiFi

    xTaskCreate(socket_server_task,
                "socket_server",
                4096,
                NULL,
                5,
                NULL);
}
