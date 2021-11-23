#ifndef OS_IO_H
#define OS_IO_H

#include <stdint.h>
#include <x86intrin.h>

#define IO_WAIT_CYCLES		20000ull

static inline void ioWait(void);
static inline uint8_t inPort8(uint16_t port);
static inline uint16_t inPort16(uint16_t port);
static inline uint32_t inPort32(uint16_t port);
static inline void outPort8(uint16_t port, uint8_t data);
static inline void outPort16(uint16_t port, uint16_t data);
static inline void outPort32(uint16_t port, uint32_t data);

static inline void ioWait(void) {
  // CPUID is a serializing innstruction
  __asm__ __volatile__("cpuid" :: "a"(0) : "eax","ebx","ecx","edx");
  unsigned long long time1 = _rdtsc();

	while(1) {
		__pause();
	  __asm__ __volatile__("cpuid" :: "a"(0) : "eax","ebx","ecx","edx");
		unsigned long long time2 = _rdtsc();

		if(time2 < time1) {
			time1 = time2;
		}
		else if(time2 - time1 >= IO_WAIT_CYCLES) {
			break;
		}
	}
}

static inline uint8_t inPort8(uint16_t port) {
	uint8_t result;
	__asm__ __volatile__("in %%dx, %%al" : "=a" (result) : "d" (port));
	return result;
}

static inline uint16_t inPort16(uint16_t port) {
	uint16_t result;
	__asm__ __volatile__("in %%dx, %%ax" : "=a" (result) : "d" (port));
	return result;
}

static inline uint32_t inPort32(uint16_t port) {
	uint32_t result;
	__asm__ __volatile__("in %%dx, %%eax" : "=a" (result) : "d" (port));
	return result;
}

static inline void outPort8(uint16_t port, uint8_t data) {
	__asm__ __volatile__("out %%al, %%dx" :: "a" (data), "d" (port));
}

static inline void outPort16(uint16_t port, uint16_t data) {
	__asm__ __volatile__("out %%ax, %%dx" :: "a" (data), "d" (port));
}

static inline void outPort32(uint16_t port, uint32_t data) {
	__asm__ __volatile__("out %%eax, %%dx" :: "a" (data), "d" (port));
}

#endif /* OS_IO_H */
