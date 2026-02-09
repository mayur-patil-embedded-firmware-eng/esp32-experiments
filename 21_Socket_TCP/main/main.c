#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "driver/gpio.h"

/* ================= USER CONFIG ================= */
#define WIFI_SSID      "TP-Link_C782"
#define WIFI_PASS      "Nayan@9173721024"

#define LED_GPIO       GPIO_NUM_2
#define SERVER_PORT    3333

#define CMD_BUFFER_LEN 64

static const char *TAG = "SOCKET_LED";

/* ================= WIFI EVENT HANDLER ================= */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
    }
}

/* ================= WIFI INIT ================= */
static void wifi_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_register(WIFI_EVENT,
                                   ESP_EVENT_ANY_ID,
                                   &wifi_event_handler,
                                   NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization complete");
}

/* ================= SOCKET SERVER TASK ================= */
static void socket_server_task(void *pvParameters)
{
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    char cmd_buffer[CMD_BUFFER_LEN];
    int cmd_index = 0;

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        vTaskDelete(NULL);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(listen_sock, 1);

    ESP_LOGI(TAG, "Socket server listening on port %d", SERVER_PORT);

    while (1) {

        int client_sock = accept(listen_sock,
                                 (struct sockaddr *)&client_addr,
                                 &addr_len);

        ESP_LOGI(TAG, "Client connected");

        cmd_index = 0;
        memset(cmd_buffer, 0, sizeof(cmd_buffer));

        while (1) {
            char ch;
            int len = recv(client_sock, &ch, 1, 0);

            if (len <= 0) {
                break;
            }

            /* Ignore carriage return */
            if (ch == '\r') {
                continue;
            }

            /* End of command */
            if (ch == '\n') {
                cmd_buffer[cmd_index] = '\0';

                ESP_LOGI(TAG, "Command received: %s", cmd_buffer);

                if (strcasecmp(cmd_buffer, "ON") == 0) {
                    gpio_set_level(LED_GPIO, 1);
                    send(client_sock, "LED ON\n", 7, 0);
                }
                else if (strcasecmp(cmd_buffer, "OFF") == 0) {
                    gpio_set_level(LED_GPIO, 0);
                    send(client_sock, "LED OFF\n", 8, 0);
                }
                else {
                    send(client_sock, "UNKNOWN CMD\n", 12, 0);
                }

                /* Reset command buffer */
                cmd_index = 0;
                memset(cmd_buffer, 0, sizeof(cmd_buffer));
            }
            else {
                if (cmd_index < CMD_BUFFER_LEN - 1) {
                    cmd_buffer[cmd_index++] = ch;
                }
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
    gpio_set_level(LED_GPIO, 0);

    wifi_init();

    /* Wait for WiFi to get IP */
    vTaskDelay(pdMS_TO_TICKS(5000));

    xTaskCreate(socket_server_task,
                "socket_server",
                4096,
                NULL,
                5,
                NULL);
}
