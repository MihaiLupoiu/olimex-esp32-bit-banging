#pragma once

#include "sdkconfig.h"

// Olimex EVB
#ifdef CONFIG_P2M_ESP32_EVB
#define HW_BOARD "OLIMEX_EVB"
#define RX_GPIO GPIO_NUM_36 // CABLE TX
#define TX_GPIO GPIO_NUM_4  // CABLE RX
#endif

// Olimex DevKitC V2
#ifdef CONFIG_P2M_ESP32_DEV_V2
#define HW_BOARD "OLIMEX_DEVKITC_V2"
#define RX_GPIO GPIO_NUM_25 // CABLE TX
#define TX_GPIO GPIO_NUM_26 // CABLE RX
#endif