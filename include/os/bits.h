#ifndef BITS_H
#define BITS_H

#define FROM_FLAG_BIT(bit)          (1u << (bit))
#define SET_FLAG(value, flag)       do { (value) |= (flag); } while(0)
#define CLEAR_FLAG(value, flag)     do { (value) &= ~(flag); } while(0)
#define TOGGLE_FLAG(value, flag)    do { (value) ^= (flag); } while (0)
#define IS_FLAG_SET(value, flag)    (((value) & (flag)) != 0)

#endif /* BITS_H */
