/* $Id: strncpy.c 262 2006-11-16 07:34:57Z solar $ */

/* Release $Name$ */

/* strncpy( char *, const char *, size_t )

 This file is part of the Public Domain C Library (PDCLib).
 Permission is granted to use, modify, and / or redistribute at will.
 */

#include <string.h>

char* strncpy(char *s1, const char *s2, size_t n) {
  char *rc = s1;
  while((n > 0) && (*s1++ = *s2++)) {
    /* Cannot do "n--" in the conditional as size_t is unsigned and we have
     to check it again for >0 in the next loop.
     */
    --n;
  }
  /* TODO: This works correctly, but somehow the handling of n is ugly as
   hell.
   */
  if(n > 0) {
    while(--n) {
      *s1++ = '\0';
    }
  }
  return rc;
}
