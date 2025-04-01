#ifndef OS_SLICE_H
#define OS_SLICE_H

#include <stddef.h>

#define SLICE(type) struct {\
    type *ptr;\
    size_t length;\
}

#define CAST_SLICE(t, s) (t) { .ptr = (void *)(s).ptr, .length = (s).length / sizeof *((t *)NULL)->ptr }

typedef SLICE(const char) ConstCharSlice;
typedef SLICE(char) CharSlice;
typedef SLICE(unsigned char) ByteSlice;

#define SLICE_FROM_ARRAY(type, arr, n)  (type) { .ptr = (arr), .length = (n) }
#define NULL_SLICE(type)   (type) { .ptr = NULL, .length = 0 }
#define SLICE_IS_NULL(s)    ((s).ptr == NULL)

#endif /* OS_SLICE_H */