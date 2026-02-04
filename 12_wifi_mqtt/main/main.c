#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

#include "driver/gpio.h"

/* ================= USER CONFIG ================= */
/* ================= User WiFi Config ================= */
#define WIFI_SSID      "TP-Link_C782"
#define WIFI_PASS      "Nayan@9173721024"

#define MQTT_BROKER_URI "mqtt://broker.hivemq.com"
#define MQTT_TOPIC      "esp/mayur/led"

#define LED_GPIO        2   // ESP32 onboard LED

static const char *TAG = "ESP_MQTT_LED";

/* ================= WIFI EVENT ================= */
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }

    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "WiFi Connected");
    }

    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGW(TAG, "WiFi Disconnected, retrying...");
    }
}

/* ================= WIFI INIT ================= */
static void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        &wifi_event_handler, NULL);

    esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
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

/* ================= MQTT EVENT ================= */
static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected");
        esp_mqtt_client_subscribe(event->client, MQTT_TOPIC, 0);
        break;

   case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "----- MQTT DATA RECEIVED -----");
    ESP_LOGI(TAG, "Topic: %.*s",
             event->topic_len, event->topic);
    ESP_LOGI(TAG, "Data : %.*s",
             event->data_len, event->data);

    if (strncmp(event->data, "ON", event->data_len) == 0) {
        gpio_set_level(LED_GPIO, 1);
        ESP_LOGI(TAG, "LED ON");
    }
    else if (strncmp(event->data, "OFF", event->data_len) == 0) {
        gpio_set_level(LED_GPIO, 0);
        ESP_LOGI(TAG, "LED OFF");
    }
    break;


    default:
        break;
    }
}

/* ================= MQTT START ================= */
static void mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    esp_mqtt_client_handle_t client =
        esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(
        client, ESP_EVENT_ANY_ID,
        mqtt_event_handler, NULL);

    esp_mqtt_client_start(client);
}

/* ================= MAIN ================= */
void app_main(void)
{
    /* NVS required for WiFi */
    nvs_flash_init();

    /* LED init */
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);
    // gpio_set_level(LED_GPIO, 1);

    /* WiFi */
    wifi_init_sta();

    /* Wait for WiFi */
    xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT,
        false,
        true,
        portMAX_DELAY);

    /* MQTT */
    mqtt_start();
}
