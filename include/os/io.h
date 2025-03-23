#ifndef OS_IO_H
#define OS_IO_H

#include <stdint.h>

static inline uint8_t in_port8(uint16_t port);
static inline uint16_t in_port16(uint16_t port);
static inline uint32_t in_port32(uint16_t port);
static inline void out_port8(uint16_t port, uint8_t data);
static inline void out_port16(uint16_t port, uint16_t data);
static inline void out_port32(uint16_t port, uint32_t data);

static inline uint8_t in_port8(uint16_t port) {
	uint8_t result;
	__asm__ __volatile__("in %%dx, %%al" : "=a" (result) : "d" (port));
	return result;
}

static inline uint16_t in_port16(uint16_t port) {
	uint16_t result;
	__asm__ __volatile__("in %%dx, %%ax" : "=a" (result) : "d" (port));
	return result;
}

static inline uint32_t in_port32(uint16_t port) {
	uint32_t result;
	__asm__ __volatile__("in %%dx, %%eax" : "=a" (result) : "d" (port));
	return result;
}

static inline void out_port8(uint16_t port, uint8_t data) {
	__asm__ __volatile__("out %%al, %%dx" :: "a" (data), "d" (port));
}

static inline void out_port16(uint16_t port, uint16_t data) {
	__asm__ __volatile__("out %%ax, %%dx" :: "a" (data), "d" (port));
}

static inline void out_port32(uint16_t port, uint32_t data) {
	__asm__ __volatile__("out %%eax, %%dx" :: "a" (data), "d" (port));
}

#endif /* OS_IO_H */
