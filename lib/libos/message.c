#include <os/msg/message.h>
#include <os/bits.h>

void setMessagePayload(const msg_t *msg) {
  if(IS_FLAG_SET(msg->flags, MSG_STD)) {
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
}

void getMessagePayload(msg_t *msg) {
  if(IS_FLAG_SET(msg->flags, MSG_STD)) {
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
}

void finishMessagePayload(void) {
}
