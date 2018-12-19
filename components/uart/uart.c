#include <string.h>
#include "i2s.h"
#include "utils.h"
#include "esp_log.h"
#include "uart.h"

// ----------------------- GLOBAL VARIABLES -----------------------
static const char *TAG = "UART";

i2s_ctx_t *i2s_ctx = NULL;
uint8_t *recv_buff = NULL;
uint8_t *send_buff = NULL;
endianess_t bit_endianess = BIT_ENDIAN_LSB;

// ring_buffer_t ctx_ringbuffer_recv;
QueueHandle_t uart_queue = NULL;
uint16_t uart_queue_size = 64;

TaskHandle_t task_process_handler;

// recv status
uint32_t pending_bytes_to_copy = 0;
uint8_t *recv_frame = NULL;
// -----------

// send status
uint8_t *send_frame = NULL;
// -----------

// ----------------------- FIN GLOBAL VARIABLES -----------------------

// ----------------------- PRIVATE METHODS -----------------------
int parse_frame(uint8_t *buff, size_t size, uint16_t *data) {
	*data = 0;

	for (size_t i = 0; i < size; i += 16) {
		uint8_t bit = _check_bit(&buff[i], 16);
		switch (i) {
		case 0:
			/* START BIT */
			// printf("START: %d ", bit);
			if (bit != 0) {
				return -1;
			}
			break;
		case 160:
			/* STOP */
			// printf("STOP: %d ", bit);
			if (bit != 1) {
				return -1;
			};
			break;

		default:
			/* DATA && MODE the last bit starts at 144 */
			// printf("D: %d ", bit);
			*data <<= 1;
			*data += bit;
			break;
		}
	}

	// TODO: Change to set bit in coresponding place
	for (size_t i = 0; i < 7; i++) {
		*data <<= 1;
		*data += 0;
	}

	if (bit_endianess == BIT_ENDIAN_LSB) {
		*data = reverse_bit16(*data);
	}
	return 0;
}

void prepare_frame(uint8_t *write_buff, uint32_t sizeElement, uint16_t data) {
	uint8_t one = 0xFF;
	uint8_t zero = 0x00;

	// Start Bit
	uint32_t pos = 0;
	memset(&write_buff[pos], zero, sizeElement);

	// TODO: Change to set bit in coresponding place
	// for (size_t i = 0; i < 8; i++) {
	// 	data <<= 1;
	// }

	// Data Bits && Mode Bit (last bit).
	if (bit_endianess == BIT_ENDIAN_LSB) {
		data = reverse_bit16(data);
	}

	for (int k = 15; k >= 7; k--) {
		pos = ((11 - (k - 5)) * sizeElement);

		uint8_t bit = (data >> k) & 0x1;

		if (bit == 1) {
			memset(&write_buff[pos], one, sizeElement);
		} else {
			memset(&write_buff[pos], zero, sizeElement);
		}
	}

	//  Stop Bit
	pos = 10 * sizeElement;
	memset(&write_buff[pos], one, sizeElement);
}

void task_process_raw_bits() {
	while (1) {
		if (process_uart_recv() < 0) {
			ESP_LOGE(TAG, "Error processing task finish with exit status != 0.");
		}
		if (uxQueueSpacesAvailable(uart_queue) < 10) {
			ESP_LOGW(TAG, "UART queue: %d", uxQueueSpacesAvailable(uart_queue));
		}
	}
}

// ----------------------- PRIVATE METHODS -----------------------

// ----------------------- PUBLIC METHODS -----------------------
void uart_init(uint32_t baud_rate, endianess_t endianess) {
	bit_endianess = endianess;

	// TODO: Config file
	i2s_ctx = i2s_new();
	i2s_ctx->baudrate = baud_rate;
	i2s_init(i2s_ctx);

	// rb_Init(&ctx_ringbuffer_recv);

	uart_queue = xQueueCreate(uart_queue_size, sizeof(uint16_t));
	if (uart_queue == NULL) {
		ESP_LOGE(TAG, "Queue not created.");
	} else {
		vQueueAddToRegistry(xQueue, "UART_Buffer");
	}

	recv_buff = calloc(i2s_ctx->bits_per_sample / 8, i2s_ctx->rx_dma_buf_length_samples);
	send_buff = calloc(i2s_ctx->raw_data_size_bytes, 1);

	// recv status
	pending_bytes_to_copy = 0;
	recv_frame = calloc(1, i2s_ctx->raw_data_size_bytes);

	// send statis
	send_frame = calloc(1, i2s_ctx->raw_data_size_bytes);

	BaseType_t taskStatus;
	taskStatus = xTaskCreate(task_process_raw_bits, "UART_Process_Tasks", 2048, NULL, 5, &task_process_handler);
	if (taskStatus != pdPASS) {
		ESP_LOGE(TAG, "Error creating UART Process Tasks");
	} else {
		ESP_LOGI(TAG, "Succesfully created UART Process Tasks");
	}
}

int uart_send(uint16_t data) {
	int ret = -1;
	prepare_frame(send_frame, i2s_ctx->bits_per_sample, data);

	if (i2s_phy_send(i2s_ctx, send_frame) != 0) {
		ret = 0;
	}
	return ret;
}

int process_uart_recv(void) {
	int ret = 0;
	uint32_t bytes_read = i2s_phy_recv(i2s_ctx, recv_buff);
	for (size_t i = 0; i < bytes_read; i++) {
		if (recv_buff[i] == 0x00 || pending_bytes_to_copy > 0) {
			int start_index = 0;
			int buffer_size_left = bytes_read - i;
			int bytes_to_copy = i2s_ctx->raw_data_size_bytes;

			if (pending_bytes_to_copy > 0) {
				start_index = i2s_ctx->raw_data_size_bytes - pending_bytes_to_copy;
				bytes_to_copy = pending_bytes_to_copy;
				pending_bytes_to_copy = 0;
			} else {
				bytes_to_copy = buffer_size_left;
				if (buffer_size_left >= i2s_ctx->raw_data_size_bytes) {
					bytes_to_copy = i2s_ctx->raw_data_size_bytes;
				}
				pending_bytes_to_copy = i2s_ctx->raw_data_size_bytes - bytes_to_copy;
			}

			memcpy(&recv_frame[start_index], &recv_buff[i], bytes_to_copy);
			if (pending_bytes_to_copy <= 0) {
				if (uxQueueSpacesAvailable(uart_queue) != 0) {
					uint16_t tmp_data = 0;
					if (parse_frame(recv_frame, i2s_ctx->raw_data_size_bytes, &tmp_data) != 0) {
						ESP_LOGE(TAG, "Unable to parse frame.");
						ret = -1;
					} else {
						// printf("--> %02X\n", tmp_data);
						// rb_Push(&ctx_ringbuffer_recv, tmp_data);
						xQueueSend(uart_queue, &tmp_data, (TickType_t) portMAX_DELAY);
						ret = uxQueueMessagesWaiting(uart_queue);
					}
				} else {
					ESP_LOGE(TAG, "Recv ring buffer is full.");
					ret = -3;
				}
				memset(recv_frame, 0, i2s_ctx->raw_data_size_bytes);
			}

			// Jump 'i' to next valid position to check.
			if (buffer_size_left >= i2s_ctx->raw_data_size_bytes) {
				i = i + bytes_to_copy + 1;
			} else {
				break;
			}
		}
	}
	memset(recv_buff, 0, i2s_ctx->i2s_rx_queue_item_size);
	return ret;
}

int uart_recv(uint16_t *data) {
	if (xQueueReceive(uart_queue, data, (TickType_t) portMAX_DELAY)) {
		return 0;
	}
	return -1;
}

uint32_t uart_get_recv_buffer_level() {
	return (uint8_t)(uxQueueMessagesWaiting(uart_queue));
}