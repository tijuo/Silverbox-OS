#ifndef KERNEL_BITS_H
#define KERNEL_BITS_H

#define FROM_FLAG_BIT(bit)          (1u << (bit))
#define SET_FLAG(value, flag)       ((value) |= (flag))
#define CLEAR_FLAG(value, flag)     ((value) &= ~(flag))
#define IS_FLAG_SET(value, flag)    \
({ \
  __typeof__ (flag) _flag=(flag); \
  ((value) & _flag) == _flag;     \
})

#endif /* KERNEL_BITS_H */
