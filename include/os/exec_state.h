#ifndef INCLUDE_OS_EXEC_STATE_H_
#define INCLUDE_OS_EXEC_STATE_H_

#include <stdint.h>

// 168 bytes

typedef struct {
  uint16_t gs;
  uint16_t fs;
  uint16_t es;
  uint16_t ds;
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rbp;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rbx;
  uint64_t rax;
  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
} exec_state_t;

_Static_assert(sizeof(exec_state_t) == 168, "exec_state_t should have a size of 168 bytes.");

// 512 bytes

struct LegacyXSaveState {
  uint16_t fcw;
  uint16_t fsw;
  uint8_t ftw;
  uint8_t _resd1;
  uint16_t fop;
  uint32_t fpuIp;
  uint16_t fpuCs;
  uint16_t _resd2;
  uint32_t fpuDp;
  uint16_t fpuDs;
  uint16_t _resd3;
  uint32_t mxcsr;
  uint32_t mxcsrMask;

  union {
    uint8_t mm0[10];
    uint8_t st0[10];
  };

  uint8_t _resd4[6];

  union {
    uint8_t mm1[10];
    uint8_t st1[10];
  };

  uint8_t _resd5[6];

  union {
    uint8_t mm2[10];
    uint8_t st2[10];
  };

  uint8_t _resd6[6];

  union {
    uint8_t mm3[10];
    uint8_t st3[10];
  };

  uint8_t _resd7[6];

  union {
    uint8_t mm4[10];
    uint8_t st4[10];
  };

  uint8_t _resd8[6];

  union {
    uint8_t mm5[10];
    uint8_t st5[10];
  };

  uint8_t _resd9[6];

  union {
    uint8_t mm6[10];
    uint8_t st6[10];
  };

  uint8_t _resd10[6];

  union {
    uint8_t mm7[10];
    uint8_t st7[10];
  };

  uint8_t _resd11[6];

  uint8_t xmm0[16];
  uint8_t xmm1[16];
  uint8_t xmm2[16];
  uint8_t xmm3[16];
  uint8_t xmm4[16];
  uint8_t xmm5[16];
  uint8_t xmm6[16];
  uint8_t xmm7[16];

  uint8_t _resd12[176];
  uint8_t available[48];
};

_Static_assert(sizeof(struct LegacyXSaveState) == 512, "LegacyXSaveState should have a size of 512 bytes");

#endif /* INCLUDE_OS_EXEC_STATE_H_ */
