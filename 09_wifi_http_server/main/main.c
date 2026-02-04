#include <string.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

/* ================= User WiFi Config ================= */
#define WIFI_SSID      "TP-Link_C782"
#define WIFI_PASS      "Nayan@9173721024"

/* ================= LED Config ================= */
#define LED_GPIO GPIO_NUM_2   // ESP32 onboard LED

static const char *TAG = "WIFI_STA";

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static httpd_handle_t server = NULL;

/* ================= Web Page ================= */
static const char *HTML_PAGE =
"<!DOCTYPE html>"
"<html lang='en'>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>ESP32 LED Control</title>"

"<style>"
"body{margin:0;font-family:Arial;"
"background:linear-gradient(135deg,#e5e7eb,#f8fafc);"
"display:flex;justify-content:center;align-items:center;height:100vh;}"

".card{background:rgba(255,255,255,0.65);"
"backdrop-filter:blur(10px);"
"-webkit-backdrop-filter:blur(10px);"
"padding:30px;border-radius:20px;"
"box-shadow:0 10px 30px rgba(0,0,0,0.15);"
"width:90%;max-width:350px;text-align:center;}"

"h2{margin-bottom:20px;color:#111827;}"

".status{margin:15px;font-size:18px;color:#1f2937;}"

".toggle{width:120px;height:60px;"
"background:#e5e7eb;border-radius:30px;"
"position:relative;margin:20px auto;cursor:pointer;"
"transition:0.3s;}"

".circle{width:52px;height:52px;background:white;"
"border-radius:50%;position:absolute;top:4px;left:4px;"
"box-shadow:0 4px 10px rgba(0,0,0,0.2);"
"transition:0.3s;}"

".on{background:#2563eb;}"          /* BLUE when ON */
".on .circle{left:64px;}"

"</style>"
"</head>"

"<body>"
"<div class='card'>"
"<h2>ESP32 LED Control</h2>"

"<div class='toggle' id='toggle'>"
"<div class='circle'></div>"
"</div>"

"<div class='status' id='status'>LED OFF</div>"
"</div>"

"<script>"
"let toggle=document.getElementById('toggle');"
"let statusText=document.getElementById('status');"
"let isOn=false;"

"function updateLED(state){"
"fetch(state?'/led/on':'/led/off');"
"isOn=state;"
"toggle.classList.toggle('on',isOn);"
"statusText.innerHTML=isOn?'LED ON':'LED OFF';"
"}"

"toggle.onclick=()=>updateLED(!isOn);"

"let startX=0;"
"toggle.addEventListener('touchstart',e=>{"
"startX=e.touches[0].clientX;"
"});"

"toggle.addEventListener('touchend',e=>{"
"let endX=e.changedTouches[0].clientX;"
"if(endX-startX>30)updateLED(true);"
"if(startX-endX>30)updateLED(false);"
"});"
"</script>"

"</body>"
"</html>";



/* ================= HTTP Handlers ================= */
esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_send(req, HTML_PAGE, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t led_on_handler(httpd_req_t *req)
{
    gpio_set_level(LED_GPIO, 1);
    httpd_resp_sendstr(req, "LED ON");
    ESP_LOGI("LED", "LED ON");
    return ESP_OK;
}

esp_err_t led_off_handler(httpd_req_t *req)
{
    gpio_set_level(LED_GPIO, 0);
    httpd_resp_sendstr(req, "LED OFF");
    ESP_LOGI("LED", "LED OFF");
    return ESP_OK;
}

/* ================= URI Config ================= */
httpd_uri_t root_uri = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = root_get_handler
};

httpd_uri_t led_on_uri = {
    .uri      = "/led/on",
    .method   = HTTP_GET,
    .handler  = led_on_handler
};

httpd_uri_t led_off_uri = {
    .uri      = "/led/off",
    .method   = HTTP_GET,
    .handler  = led_off_handler
};

/* ================= Start Web Server ================= */
void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &led_on_uri);
        httpd_register_uri_handler(server, &led_off_uri);
        ESP_LOGI("HTTP", "Web server started");
    }
}

/* ================= WiFi Event Handler ================= */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi started, connecting...");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGW(TAG, "Disconnected, retrying...");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        start_webserver();
    }
}

/* ================= WiFi Init ================= */
void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init finished.");
}

/* ================= Main ================= */
void app_main(void)
{
    /* LED GPIO Init */
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0); // LED OFF initially

    wifi_init();

    xEventGroupWaitBits(wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        false,
                        true,
                        portMAX_DELAY);

    ESP_LOGI(TAG, "WiFi connected successfully!");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
