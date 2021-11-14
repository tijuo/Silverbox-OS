/* $Id: strncmp.c 262 2006-11-16 07:34:57Z solar $ */

/* Release $Name$ */

/* strncmp( const char *, const char *, size_t )

 This file is part of the Public Domain C Library (PDCLib).
 Permission is granted to use, modify, and / or redistribute at will.
 */

#include <string.h>

int strncmp(const char *s1, const char *s2, size_t n) {
  while(*s1 && n && (*s1 == *s2)) {
    ++s1;
    ++s2;
    --n;
  }
  if((n == 0)) {
    return 0;
  }
  else {
    return (*s1 - *s2);
  }
}
