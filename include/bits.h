#ifndef BITS_H
#define BITS_H

#define FROM_FLAG_BIT(bit)          (1ul << (bit))
#define SET_FLAG(value, flag)       ((value) |= (flag))
#define CLEAR_FLAG(value, flag)     ((value) &= ~(flag))
#define TOGGLE_FLAG(value, flag)    ((value) ^= (flag))
#define IS_FLAG_CLEAR(value, flag)  (!((value) & (flag)))
#define IS_FLAG_SET(value, flag)    !IS_FLAG_CLEAR(value, flag)

#endif /* BITS_H */
