#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef struct {
	uint32_t baudrate;
	uint8_t bit_banging_samples_per_bit;

	uint8_t bits_per_sample;
	uint8_t data_size_bits;
	uint16_t raw_data_size_bytes;

	uint16_t rx_dma_buf_count;          // Number of DMA buffers
	uint16_t rx_dma_buf_length_samples; // Number of samples in DMA

	uint16_t tx_dma_buf_count;          // Number of DMA buffers
	uint16_t tx_dma_buf_length_samples; // Number of samples in DMA

	QueueHandle_t i2s_rx_event_queue;
	QueueHandle_t i2s_tx_event_queue;

	TaskHandle_t task_i2s_reader_handler;
	TaskHandle_t task_i2s_writer_handler;

	QueueHandle_t i2s_tx_queue;
	uint16_t i2s_tx_queue_size;
	uint16_t i2s_tx_queue_item_size;

	QueueHandle_t i2s_rx_queue;
	uint16_t i2s_rx_queue_size;
	uint16_t i2s_rx_queue_item_size;
} i2s_ctx_t;

i2s_ctx_t *i2s_new();
void i2s_init(i2s_ctx_t *ctx);
// TODO: Queue not necessary since we need to always read from the physical layer
int i2s_phy_recv(void *pvParams, uint8_t *buff);
int i2s_phy_send(void *pvParams, uint8_t *buff);