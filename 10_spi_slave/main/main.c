//#############################################################################
//
// FILE:   spi_slave_dma_example.c
//
//! \brief Example: ESP32 SPI Slave with DMA-capable buffers
//!
//! This example shows how to configure an ESP32 as SPI slave using DMA-capable
//! buffers. It receives 16-bit data from SPI master and echoes it back.
//!
//#############################################################################

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/spi_slave.h"
#include "esp_heap_caps.h"

#define TAG "SPI_SLAVE"

// SPI pins
#define PIN_NUM_MISO  19
#define PIN_NUM_MOSI  23
#define PIN_NUM_CLK   18
#define PIN_NUM_CS    5

// DMA-capable buffers
uint16_t* tx_data;
uint16_t* rx_data;

void app_main(void)
{
    esp_err_t ret;

    // Allocate DMA-capable buffers
    tx_data = heap_caps_malloc(sizeof(uint16_t), MALLOC_CAP_DMA);
    rx_data = heap_caps_malloc(sizeof(uint16_t), MALLOC_CAP_DMA);

    if (!tx_data || !rx_data) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffers!");
        return;
    }

    // Initialize TX value
    //*tx_data = 0x1234;

    // SPI slave interface configuration
    spi_slave_interface_config_t slvcfg = {
        .mode = 0,              // SPI mode 0
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 3,        // Number of transactions queued
        .flags = 0,
        .post_setup_cb = NULL,
        .post_trans_cb = NULL
    };

    // SPI bus configuration
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = sizeof(uint16_t)
    };

    // Initialize SPI slave
    ret = spi_slave_initialize(VSPI_HOST, &buscfg, &slvcfg, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI slave: %d", ret);
        return;
    }

    ESP_LOGI(TAG, "SPI Slave initialized successfully");

    while (1) {
        // Prepare transaction
        spi_slave_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = 16;             // 16-bit transfer
        t.tx_buffer = tx_data;
        t.rx_buffer = rx_data;

        // Queue and wait for SPI transaction
        ret = spi_slave_transmit(VSPI_HOST, &t, portMAX_DELAY);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Received: 0x%04X", *rx_data);

            // Echo back received data
            *tx_data = *rx_data;
        } else {
            ESP_LOGE(TAG, "SPI transaction failed: %d", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(1)); // small delay
    }

    // Free buffers (never reached in this example)
    heap_caps_free(tx_data);
    heap_caps_free(rx_data);
}
