#ifndef BITS_H
#define BITS_H

#define LONG_FROM_FLAG_BIT(bit)     (1ul << (bit))
#define FROM_FLAG_BIT(bit)          (1u << (bit))
#define SET_FLAG(value, flag)       ((value) |= (flag))
#define CLEAR_FLAG(value, flag)     ((value) &= ~(flag))
#define IS_FLAG_SET(value, flag)    ({ ((value) & (flag)) != 0; })

#define IS_POWER_OF_TWO(x)  (((x) & ((x) - 1)) == 0 && ((x) != 0))
#endif /* BITS_H */
