#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include <esp_system.h>
#include "esp_log.h"
#include <string.h>
#include "boards.h"

#include "uart.h"

#ifndef HW_BOARD
#define HW_BOARD "UNKOWN"
#endif

// TEST: picocom -b 9600 /dev/ttyUSB1 -p 2

void app_main() {
	ESP_LOGI("Main", "> Starting in " HW_BOARD "\n");
	ESP_LOGI("Main", "Init DONE!");

	uart_init(9600, BIT_ENDIAN_LSB);
	uint16_t data = 0;

	while (1) {
		if (uart_recv(&data) == 0) {
			printf(">>>>>> %d -> %c \n", data & 0xFF, data & 0xFF);
			uart_send(data);
		}
	}

	return;
}