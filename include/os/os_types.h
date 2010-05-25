#ifndef OS_TYPES_H
#define OS_TYPES_H

#include <os/ostypes/sbarray.h>
#include <os/ostypes/sbstring.h>
#include <os/ostypes/sbassocarray.h>

struct _SBRange
{
  int start;
  int length;
};

typedef struct _SBRange SBRange;

#endif /* OS_TYPES_H */
