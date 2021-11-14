#include <os/msg/message.h>
#include <os/bits.h>

void setMessagePayload(const msg_t *msg) {
  if(IS_FLAG_SET(msg->flags, MSG_STD) || IS_FLAG_SET(msg->flags, MSG_HUGE)) {
    // Store payload in MMX registers
    __asm__("movq (%%eax), %%mm0\n"
            "movq 8(%%eax), %%mm1\n"
            "movq 16(%%eax), %%mm2\n"
            "movq 24(%%eax), %%mm3\n"
            "movq 32(%%eax), %%mm4\n"
            "movq 40(%%eax), %%mm5\n"
            "movq 48(%%eax), %%mm6\n"
            "movq 56(%%eax), %%mm7\n"
            :: "a"(msg) : "mm0", "mm1", "mm2", "mm3",
                          "mm4", "mm5", "mm6", "mm7", "memory");
  }

  if(IS_FLAG_SET(msg->flags, MSG_LONG)) {
    // Store payload in SSE registers
    __asm__("movups (%%eax), %%xmm0\n"
            "movups 16(%%eax), %%xmm1\n"
            "movups 32(%%eax), %%xmm2\n"
            "movups 48(%%eax), %%xmm3\n"
            "movups 64(%%eax), %%xmm4\n"
            "movups 80(%%eax), %%xmm5\n"
            "movups 96(%%eax), %%xmm6\n"
            "movups 112(%%eax), %%xmm7\n"
            :: "a"(msg) : "xmm0", "xmm1", "xmm2", "xmm3",
                          "xmm4", "xmm5", "xmm6", "xmm7", "memory");
  }
  else if(IS_FLAG_SET(msg->flags, MSG_HUGE)) {
    // Store payload in both MMX and SSE registers (MMX, first)
    __asm__("movups 64(%%eax), %%xmm0\n"
            "movups 80(%%eax), %%xmm1\n"
            "movups 96(%%eax), %%xmm2\n"
            "movups 112(%%eax), %%xmm3\n"
            "movups 128(%%eax), %%xmm4\n"
            "movups 144(%%eax), %%xmm5\n"
            "movups 160(%%eax), %%xmm6\n"
            "movups 176(%%eax), %%xmm7\n"
            :: "a"(msg) : "xmm0", "xmm1", "xmm2", "xmm3",
                          "xmm4", "xmm5", "xmm6", "xmm7", "memory");
  }
}

void getMessagePayload(msg_t *msg) {
  if(IS_FLAG_SET(msg->flags, MSG_STD) || IS_FLAG_SET(msg->flags, MSG_HUGE)) {
    // Store payload in MMX registers
    __asm__("movq %%mm0, (%%eax)\n"
            "movq %%mm1, 8(%%eax)\n"
            "movq %%mm2, 16(%%eax)\n"
            "movq %%mm3, 24(%%eax)\n"
            "movq %%mm4, 32(%%eax)\n"
            "movq %%mm5, 40(%%eax)\n"
            "movq %%mm6, 48(%%eax)\n"
            "movq %%mm7, 56(%%eax)\n"
            :: "a"(msg) : "mm0", "mm1", "mm2", "mm3",
                          "mm4", "mm5", "mm6", "mm7", "memory");
  }

  if(IS_FLAG_SET(msg->flags, MSG_LONG)) {
    // Store payload in SSE registers
    __asm__("movups %%xmm0, (%%eax)\n"
            "movups %%xmm1, 16(%%eax)\n"
            "movups %%xmm2, 32(%%eax)\n"
            "movups %%xmm3, 48(%%eax)\n"
            "movups %%xmm4, 64(%%eax)\n"
            "movups %%xmm5, 80(%%eax)\n"
            "movups %%xmm6, 96(%%eax)\n"
            "movups %%xmm7, 112(%%eax)\n"
            :: "a"(msg) : "xmm0", "xmm1", "xmm2", "xmm3",
                          "xmm4", "xmm5", "xmm6", "xmm7", "memory");
  }
  else if(IS_FLAG_SET(msg->flags, MSG_HUGE)) {
    // Store payload in both MMX and SSE registers (MMX, first)
    __asm__("movups %%xmm0, 64(%%eax)\n"
            "movups %%xmm1, 80(%%eax)\n"
            "movups %%xmm2, 96(%%eax)\n"
            "movups %%xmm3, 112(%%eax)\n"
            "movups %%xmm4, 128(%%eax)\n"
            "movups %%xmm5, 144(%%eax)\n"
            "movups %%xmm6, 160(%%eax)\n"
            "movups %%xmm7, 176(%%eax)\n"
            :: "a"(msg) : "xmm0", "xmm1", "xmm2", "xmm3",
                          "xmm4", "xmm5", "xmm6", "xmm7", "memory");
  }
}

void finishMessagePayload(void) {
  __asm__ __volatile__("emms");
}
