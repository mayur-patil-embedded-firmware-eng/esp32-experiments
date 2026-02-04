/************************************************************
 * ESP32 BLE ADC READ Example (Single File – FINAL)
 * - Custom UUID
 * - Correct READ response
 * - Visible & readable in nRF Connect
 ************************************************************/

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "driver/adc.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

/* -------------------- DEFINES -------------------- */
#define TAG "BLE_ADC"

#define DEVICE_NAME  "ESP32_ADC"

/* Custom UUIDs (DO NOT use SIG UUIDs for custom data) */
#define SERVICE_UUID 0xFFF0
#define CHAR_UUID    0xFFF1

/* -------------------- GLOBALS -------------------- */
static uint16_t adc_value = 0;
static uint16_t service_handle;
static esp_gatt_if_t gatt_if_global;

/* -------------------- ADVERTISING -------------------- */
static esp_ble_adv_params_t adv_params = {
    .adv_int_min       = 0x20,
    .adv_int_max       = 0x40,
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* -------------------- GAP CALLBACK -------------------- */
static void gap_event_handler(esp_gap_ble_cb_event_t event,
                              esp_ble_gap_cb_param_t *param)
{
    if (event == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT)
    {
        esp_ble_gap_start_advertising(&adv_params);
        ESP_LOGI(TAG, "Advertising started");
    }
}

/* -------------------- GATT CALLBACK -------------------- */
static void gatts_event_handler(esp_gatts_cb_event_t event,
                                esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
        case ESP_GATTS_REG_EVT:
            gatt_if_global = gatts_if;

            esp_ble_gap_set_device_name(DEVICE_NAME);

            esp_ble_gap_config_adv_data(&(esp_ble_adv_data_t){
                .set_scan_rsp = false,
                .include_name = true,
                .flag = ESP_BLE_ADV_FLAG_GEN_DISC |
                        ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
            });

            esp_ble_gatts_create_service(
                gatts_if,
                &(esp_gatt_srvc_id_t){
                    .is_primary = true,
                    .id.inst_id = 0,
                    .id.uuid.len = ESP_UUID_LEN_16,
                    .id.uuid.uuid.uuid16 = SERVICE_UUID,
                },
                4);
            break;

        case ESP_GATTS_CREATE_EVT:
            service_handle = param->create.service_handle;

            esp_ble_gatts_start_service(service_handle);

            esp_ble_gatts_add_char(
                service_handle,
                &(esp_bt_uuid_t){
                    .len = ESP_UUID_LEN_16,
                    .uuid.uuid16 = CHAR_UUID,
                },
                ESP_GATT_PERM_READ,
                ESP_GATT_CHAR_PROP_BIT_READ,
                NULL,   /* value handled manually on READ */
                NULL);
            break;

        /* 🔑 MOST IMPORTANT FIX */
       case ESP_GATTS_READ_EVT:
		{
    		esp_gatt_rsp_t rsp;
    		memset(&rsp, 0, sizeof(esp_gatt_rsp_t));

    		uint16_t adc_le;

   		 /* ADC is 12-bit → mask upper bits */
    		adc_le = adc_value & 0x0FFF;

    		rsp.attr_value.handle = param->read.handle;
    		rsp.attr_value.len    = sizeof(uint16_t);

    		/* LITTLE ENDIAN (BLE standard for numbers) */
    		rsp.attr_value.value[0] = (uint8_t)(adc_le & 0xFF);        // LSB
    		rsp.attr_value.value[1] = (uint8_t)((adc_le >> 8) & 0xFF); // MSB

    		esp_ble_gatts_send_response(
        		gatts_if,
       		 param->read.conn_id,
       		 param->read.trans_id,
       		 ESP_GATT_OK,
       		 &rsp
    		);

    		ESP_LOGI(TAG, "BLE READ -> ADC = %d (0x%04X)", adc_le, adc_le);
    		break;
		}

        default:
            break;
    }
}

/* -------------------- ADC TASK -------------------- */
static void adc_task(void *arg)
{
    while (1)
    {
        adc_value = adc1_get_raw(ADC1_CHANNEL_6);
        ESP_LOGI(TAG, "ADC Sampled: %d", adc_value);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* -------------------- MAIN -------------------- */
void app_main(void)
{
    /* NVS required for BLE */
    nvs_flash_init();

    /* ADC configuration (GPIO 34) */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);

    /* BLE init */
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_init(
		
        &(esp_bt_controller_config_t)BT_CONTROLLER_INIT_CONFIG_DEFAULT());
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    esp_bluedroid_init();
    esp_bluedroid_enable();

    /* Register callbacks */
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gatts_app_register(0);

    /* Start ADC sampling task */
    xTaskCreate(adc_task, "adc_task", 2048, NULL, 5, NULL);
}
