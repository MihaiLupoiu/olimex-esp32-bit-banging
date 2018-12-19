#pragma once
#include <inttypes.h>

typedef enum { BIT_ENDIAN_LSB = 0, BIT_ENDIAN_MSB = 1 } endianess_t;

void uart_init(uint32_t baud_rate, endianess_t endianess);
int uart_send(uint16_t data);
int process_uart_recv(void);
int uart_recv(uint16_t *data);
uint32_t uart_get_recv_buffer_level();
