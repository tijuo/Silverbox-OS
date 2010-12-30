#ifndef OS_TYPES_H
#define OS_TYPES_H

#include <os/ostypes/sbarray.h>
#include <os/ostypes/sbstring.h>
#include <os/ostypes/sbassocarray.h>

/* enum SBObjectType { SB_ARRAY, SB_STRING, SB_ASSOC_ARRAY, SB_RANGE };*/

struct _SBRange
{
  int start;
  int length;
};

typedef struct _SBRange SBRange;

/*
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
*/


#endif /* OS_TYPES_H */
