#ifndef OS_TYPES_H
#define OS_TYPES_H

#include <os/ostypes/sbarray.h>
#include <os/ostypes/sbstring.h>
#include <os/ostypes/sbassocarray.h>

enum SBObjectType { SB_ARRAY, SB_STRING, SB_ASSOC_ARRAY };

typedef struct
{
  enum SBObjectType type;

  union ObjectData
  {
    SBArray array;
    SBString string;
    SBAssocArray assocArray;
    SBRange range;
  };
} SBObject;

struct _SBRange
{
  int start;
  int length;
};

typedef struct _SBRange SBRange;

#endif /* OS_TYPES_H */
