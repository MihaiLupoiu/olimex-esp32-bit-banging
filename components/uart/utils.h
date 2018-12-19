#pragma once
#include <inttypes.h>
#include <sys/time.h>

uint8_t reverse_bit8(uint8_t x);
uint16_t reverse_bit16(uint16_t x);
uint32_t reverse_bit32(uint32_t x);

void printUint16Binary(uint16_t data);
uint32_t _countSetBits(uint8_t n);
uint8_t _check_bit(uint8_t *buff, uint32_t buff_size);

time_t getMicrotime();
