#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_http_server.h"
#include "esp_ota_ops.h"

#include "driver/gpio.h"

/* ================= CONFIG ================= */

#define WIFI_SSID  "TP-Link_C782"
#define WIFI_PASS  "Nayan@9173721024"

#define LED_GPIO   GPIO_NUM_2

static const char *TAG = "OTA_FACTORY";

/* ========================================== */

/* -------- HTML PAGE -------- */
static const char *html_page =
"<!DOCTYPE html><html>"
"<head><title>ESP32 OTA</title></head>"
"<body>"
"<h2>ESP32 OTA Update</h2>"
"<form method='POST' action='/update' enctype='multipart/form-data'>"
"<input type='file' name='firmware' />"
"<input type='submit' value='Upload' />"
"</form>"
"</body></html>";

/* -------- ROOT PAGE -------- */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* -------- OTA HANDLER -------- */
static esp_err_t ota_post_handler(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle;
    const esp_partition_t *update_partition =
        esp_ota_get_next_update_partition(NULL);

    if (!update_partition) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "No OTA partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA writing to: %s", update_partition->label);
    ESP_ERROR_CHECK(esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle));

    char buf[1024];
    int received;
    bool header_skipped = false;

    while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0) {

        char *data = buf;
        int data_len = received;

        /* 🔥 Skip multipart headers ONCE */
        if (!header_skipped) {
            char *body = strstr(buf, "\r\n\r\n");
            if (body) {
                body += 4;
                data_len = received - (body - buf);
                data = body;
                header_skipped = true;
            } else {
                continue; // still in headers
            }
        }

        ESP_ERROR_CHECK(esp_ota_write(ota_handle, data, data_len));
    }

    ESP_ERROR_CHECK(esp_ota_end(ota_handle));
    ESP_ERROR_CHECK(esp_ota_set_boot_partition(update_partition));

    httpd_resp_sendstr(req, "OTA OK. Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}



/* -------- WEB SERVER -------- */
static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t root = {
        .uri     = "/",
        .method  = HTTP_GET,
        .handler = root_get_handler
    };

    httpd_uri_t update = {
        .uri     = "/update",
        .method  = HTTP_POST,
        .handler = ota_post_handler
    };

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &update);

    ESP_LOGI(TAG, "OTA Web Server Started");
}

/* -------- WIFI -------- */
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

/* -------- LED TASK -------- */
static void led_task(void *arg)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));  // SLOW blink (factory)
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* -------- MAIN -------- */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();

    vTaskDelay(pdMS_TO_TICKS(5000)); // wait for IP

    esp_netif_ip_info_t ip;
    esp_netif_t *netif =
        esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    esp_netif_get_ip_info(netif, &ip);

    ESP_LOGI(TAG, "ESP32 IP: " IPSTR, IP2STR(&ip.ip));
    ESP_LOGI(TAG, "Open browser: http://" IPSTR "/", IP2STR(&ip.ip));

    start_webserver();

    xTaskCreate(led_task, "led_task", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "Factory App Running (SLOW BLINK)");
}
