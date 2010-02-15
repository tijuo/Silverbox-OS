#ifndef OS_TYPES_H
#define OS_TYPES_H

#include "sbarray.h"
#include "sbstring.h"

struct _SBRange
{
  int start;
  int length;
};

typedef struct _SBRange SBRange;

#endif /* OS_TYPES_H */
