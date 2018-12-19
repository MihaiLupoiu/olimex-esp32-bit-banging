#include "utils.h"
#include <inttypes.h>
#include <stdio.h>
#include <sys/time.h>

uint8_t reverse_bit8(uint8_t x) {
	x = ((x & 0x55) << 1) | ((x & 0xAA) >> 1);
	x = ((x & 0x33) << 2) | ((x & 0xCC) >> 2);
	return (x << 4) | (x >> 4);
}

uint16_t reverse_bit16(uint16_t x) {
	x = ((x & 0x5555) << 1) | ((x & 0xAAAA) >> 1);
	x = ((x & 0x3333) << 2) | ((x & 0xCCCC) >> 2);
	x = ((x & 0x0F0F) << 4) | ((x & 0xF0F0) >> 4);
	return (x << 8) | (x >> 8);
}

uint32_t reverse_bit32(uint32_t x) {
	x = ((x & 0x55555555) << 1) | ((x & 0xAAAAAAAA) >> 1);
	x = ((x & 0x33333333) << 2) | ((x & 0xCCCCCCCC) >> 2);
	x = ((x & 0x0F0F0F0F) << 4) | ((x & 0xF0F0F0F0) >> 4);
	x = ((x & 0x00FF00FF) << 8) | ((x & 0xFF00FF00) >> 8);
	return (x << 16) | (x >> 16);
}

void printUint16Binary(uint16_t data) {
	for (int i = 0; i < 16; i++) {
		printf("%d", (data & 0x8000) >> 15);
		data <<= 1;
	}
	printf("\n");
}

/* Function to get no of set bits in binary
   representation of passed binary no. */
// Reference: https://www.geeksforgeeks.org/count-set-bits-in-an-integer
uint32_t _count_set_bits(uint8_t n) {
	uint32_t count = 0;
	while (n) {
		n &= (n - 1);
		count++;
	}
	return count;
}

uint8_t _check_bit(uint8_t *buff, uint32_t buff_size) {
	uint32_t zeros = 0;
	uint32_t ones = 0;

	for (uint32_t i = 0; i < buff_size; i++) {
		uint32_t tmp_ones = _count_set_bits(buff[i]);
		zeros += (8 - tmp_ones);
		ones += tmp_ones;
	}

	// Zeros/ total > 0.75? (TOtal 128, 75%=96)
	// printf("0: %d - 1: %d\n", zeros, ones);
	if (zeros > 96) {
		return 0;
	}
	return 1;
}

/**
 * Returns the current time in microseconds.
 */
time_t get_microtime() {
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return currentTime.tv_sec * (int) 1e6 + currentTime.tv_usec;
}

time_t get_timestamp() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	time_t ms = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
	return ms;
}