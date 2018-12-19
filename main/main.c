#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include <esp_system.h>
#include "esp_log.h"
#include <string.h>
#include "boards.h"

#ifndef HW_BOARD
#define HW_BOARD "UNKOWN"
#endif

void app_main() {
	// Setup Watch Dog
	ESP_ERROR_CHECK(esp_task_wdt_init(20, true));
	ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
	ESP_ERROR_CHECK(esp_task_wdt_reset());

	ESP_LOGI("Main", "> Starting in " HW_BOARD "\n");
	ESP_LOGI("Main", "Init DONE!");

	while (1) {
		// Feed Watch Dog -- Not working as expected...
		ESP_ERROR_CHECK(esp_task_wdt_reset());
	}

	return;
}