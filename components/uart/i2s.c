#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "i2s.h"
#include "esp_log.h"

#include <string.h>
#include "boards.h"

static const char *TAG = "I2S";

void task_i2s_reader(void *pvParams);
void task_i2s_writer(void *pvParams);

i2s_ctx_t *i2s_new() {
	// Default UART config
	i2s_ctx_t *ctx = calloc(1, sizeof(i2s_ctx_t));
	ctx->baudrate = 9600;
	ctx->bit_banging_samples_per_bit = 8;

	ctx->bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
	ctx->data_size_bits = 11;

	ctx->task_i2s_reader_handler = NULL;
	ctx->task_i2s_writer_handler = NULL;

	ctx->rx_dma_buf_count = 2;
	ctx->rx_dma_buf_length_samples = ctx->data_size_bits * ctx->bit_banging_samples_per_bit;

	ctx->tx_dma_buf_length_samples = 8;
	ctx->raw_data_size_bytes = ctx->data_size_bits * ctx->bit_banging_samples_per_bit * (ctx->bits_per_sample / 8);
	ctx->tx_dma_buf_count = (ctx->raw_data_size_bytes / ctx->tx_dma_buf_length_samples / 2) + 1;

	ctx->i2s_rx_queue_size = 128;
	ctx->i2s_rx_queue_item_size = ctx->rx_dma_buf_length_samples * (ctx->bits_per_sample / 8);
	ctx->i2s_tx_queue_size = 128;
	ctx->i2s_tx_queue_item_size = ctx->raw_data_size_bytes;

	return ctx;
}

void i2s_init(i2s_ctx_t *ctx) {
	gpio_set_direction(RX_GPIO, GPIO_MODE_INPUT);
	gpio_set_pull_mode(RX_GPIO, GPIO_PULLUP_ONLY);

	// -- init RX
	i2s_config_t i2s_config_rx = {
		mode : (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
		sample_rate : ctx->baudrate * ctx->bit_banging_samples_per_bit,
		bits_per_sample : ctx->bits_per_sample,
		channel_format : I2S_CHANNEL_FMT_ONLY_RIGHT,
		communication_format : (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S),
		intr_alloc_flags : ESP_INTR_FLAG_LEVEL1,
		dma_buf_count : ctx->rx_dma_buf_count,
		dma_buf_len : ctx->rx_dma_buf_length_samples
	};

	i2s_pin_config_t pin_config_rx = {.bck_io_num = I2S_PIN_NO_CHANGE,
	                                  .ws_io_num = I2S_PIN_NO_CHANGE,
	                                  .data_out_num = I2S_PIN_NO_CHANGE,
	                                  .data_in_num = RX_GPIO};

	i2s_driver_install(I2S_NUM_0, &i2s_config_rx, 0, NULL);
	i2s_set_pin(I2S_NUM_0, &pin_config_rx);

	i2s_start(I2S_NUM_0);

	// -- init TX
	gpio_set_direction(TX_GPIO, GPIO_MODE_OUTPUT_OD);
	gpio_set_pull_mode(TX_GPIO, GPIO_PULLUP_ONLY);

	i2s_config_t i2s_config_tx = {
		mode : (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
		sample_rate : ctx->baudrate * ctx->bit_banging_samples_per_bit,
		bits_per_sample : ctx->bits_per_sample,
		channel_format : I2S_CHANNEL_FMT_ONLY_RIGHT,
		communication_format : (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S),
		intr_alloc_flags : ESP_INTR_FLAG_LEVEL1,
		dma_buf_count : ctx->tx_dma_buf_count,
		dma_buf_len : ctx->tx_dma_buf_length_samples
	};

	i2s_pin_config_t pin_config_tx = {.bck_io_num = I2S_PIN_NO_CHANGE,
	                                  .ws_io_num = I2S_PIN_NO_CHANGE,
	                                  .data_out_num = TX_GPIO,
	                                  .data_in_num = I2S_PIN_NO_CHANGE};

	i2s_driver_install(I2S_NUM_1, &i2s_config_tx, 4, &ctx->i2s_tx_event_queue);
	i2s_set_pin(I2S_NUM_1, &pin_config_tx);

	i2s_start(I2S_NUM_1);

	// -- create Queue and Tasks
	BaseType_t taskStatus;

	ctx->i2s_tx_queue = xQueueCreate(ctx->i2s_tx_queue_size, ctx->i2s_tx_queue_item_size);
	ctx->i2s_rx_queue = xQueueCreate(ctx->i2s_rx_queue_size, ctx->i2s_rx_queue_item_size);

	if (ctx->i2s_tx_queue == NULL || ctx->i2s_rx_queue == NULL) {
		ESP_LOGE(TAG, "Queues not created");
	} else {
		ESP_LOGD(TAG, "Success creating queues");
		//--------------------------------------------------
		taskStatus =
		xTaskCreate(task_i2s_reader, "i2s_reader_Tasks", 4096, (void *) ctx, 5, &ctx->task_i2s_reader_handler);
		if (taskStatus != pdPASS) {
			ESP_LOGE(TAG, "Error creating reader Task");
		}
		//--------------------------------------------------
		taskStatus =
		xTaskCreate(task_i2s_writer, "i2s_writer_Tasks", 2048, (void *) ctx, 5, &ctx->task_i2s_writer_handler);
		if (taskStatus != pdPASS) {
			ESP_LOGE(TAG, "Error creating i2s writer Task");
		}
	}
}

void task_i2s_reader(void *pvParams) {
	i2s_ctx_t *ctx = (i2s_ctx_t *) pvParams;

	uint16_t read_buff_size_bytes = ctx->i2s_rx_queue_item_size;
	uint8_t *buff = calloc(ctx->bits_per_sample / 8, ctx->rx_dma_buf_length_samples);

	ESP_LOGI(TAG, "RX Queue space: %d - buffer size: %d", uxQueueSpacesAvailable(ctx->i2s_rx_queue),
	         read_buff_size_bytes);

	while (1) {
		uint32_t bytes_read = 0;
		esp_err_t err;

		err = i2s_read(I2S_NUM_0, buff, read_buff_size_bytes, &(bytes_read), portMAX_DELAY - 1);
		ESP_ERROR_CHECK(err);

		if (bytes_read == read_buff_size_bytes) {
			if (xQueueSend(ctx->i2s_rx_queue, buff, (TickType_t) portMAX_DELAY) != pdTRUE) {
				ESP_LOGE(TAG, "RX Queue Full space: %d", uxQueueSpacesAvailable(ctx->i2s_rx_queue));
			}
		} else {
			ESP_LOGE(TAG, "Reading from i2s. Pending by bytes to read %d", read_buff_size_bytes - bytes_read);
		}

		if (uxQueueSpacesAvailable(ctx->i2s_rx_queue) < 10) {
			ESP_LOGW(TAG, "RX Queue space: %d - buffer size: %d", uxQueueSpacesAvailable(ctx->i2s_rx_queue),
			         read_buff_size_bytes);
		}
	}
}

void task_i2s_writer(void *pvParams) {
	// Task dedicated to write ones on the line all the time.
	// This is because the IDE status of the line is to have voltage.
	i2s_ctx_t *ctx = (i2s_ctx_t *) pvParams;
	uint32_t bytes_wrote = 0;

	esp_err_t werr;
	size_t dma_buff_size_bytes = ctx->tx_dma_buf_length_samples * ctx->bits_per_sample / 8;

	uint8_t *data = calloc(ctx->raw_data_size_bytes, 1);

	uint8_t *write_buff = (uint8_t *) calloc(ctx->raw_data_size_bytes, 1);
	memset(write_buff, 0xff, ctx->raw_data_size_bytes);

	while (1) {
		if (ctx->i2s_tx_queue != 0) {
			if (xQueueReceive(ctx->i2s_tx_queue, data, 0)) {
				// printf("Reading data from queue\n");

				werr = i2s_write(I2S_NUM_1, (void *) data, ctx->raw_data_size_bytes, &(bytes_wrote), portMAX_DELAY - 1);
				ESP_ERROR_CHECK(werr);

				// ESP_LOG_BUFFER_HEXDUMP("Frame", data, ctx->raw_data_size_bytes, ESP_LOG_WARN);

				continue;
			} else {
				werr = i2s_write(I2S_NUM_1, (void *) write_buff, dma_buff_size_bytes, &(bytes_wrote), portMAX_DELAY);
				ESP_ERROR_CHECK(werr);
			}

			i2s_event_t evt = {0};
			if (xQueueReceive(ctx->i2s_tx_event_queue, &evt, portMAX_DELAY)) {
				switch (evt.type) {
				case I2S_EVENT_TX_DONE:
					// ESP_LOGI(TAG,"I2S TX DMA Buffer Underflow \n");
					break;
				case I2S_EVENT_DMA_ERROR:
					ESP_LOGE(TAG, "TX I2S_EVENT_DMA_ERROR \n");
					break;
				case I2S_EVENT_MAX:
					ESP_LOGE(TAG, "TX I2S_EVENT_MAX \n");
					break;
				default:
					break;
				}
			}
		}
	}
}

// TODO: Queue not necessary since we need to always read from the physical layer
int i2s_phy_recv(void *pvParams, uint8_t *buff) {
	i2s_ctx_t *ctx = (i2s_ctx_t *) pvParams;

	if (ctx->i2s_rx_queue != 0) {
		if (xQueueReceive(ctx->i2s_rx_queue, buff, (TickType_t) portMAX_DELAY) != pdTRUE) {
			ESP_LOGE(TAG, "Error reading data from i2s_rx_queue");
			return -1;
		}
	}
	return ctx->i2s_rx_queue_item_size;
}

int i2s_phy_send(void *pvParams, uint8_t *buff) {
	i2s_ctx_t *ctx = (i2s_ctx_t *) pvParams;

	if (ctx->i2s_tx_queue != 0) {
		if (xQueueSend(ctx->i2s_tx_queue, buff, (TickType_t) portMAX_DELAY) != pdTRUE) {
			ESP_LOGE(TAG, "Error writing data to i2s_tx_queue. Queue probably full");
			return ctx->raw_data_size_bytes;
		}
	}
	return 0;
}