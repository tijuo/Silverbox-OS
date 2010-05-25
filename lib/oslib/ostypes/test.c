#include "sbarray.h"
#include "sbassocarray.h"
#include "sbstring.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
  int a=2,b=66,c=9;
  int *d, *e, *f;
  int *g, *h, *i;
  char *str = "Hello", *str2, *str3 = "Wh<q>at<q>is<q> th<q>e da<q>te today?", *str4;

  SBArray arr;
//  SBAssocArray arr2;
  SBString string, string2, string3;

/*
  sbAssocArrayCreate(&arr2, 10);

  sbAssocArrayInsert(&arr2, str, strlen(str), &c,sizeof(str));
  sbAssocArrayInsert(&arr2, str3, strlen(str3), &a, sizeof(str3));
  sbAssocArrayInsert(&arr2, &b, sizeof(b), str3,sizeof(b));

  sbArrayCreate(&arr,3, &a, str, &b);

  sbArrayElemAt(&arr, 0, (void **)&d);
  sbArrayElemAt(&arr, 1, (void **)&str2);
  sbArrayElemAt(&arr, 2, (void **)&e);

  sbAssocArrayLookup(&arr2, str, strlen(str), (void **)&g);
  sbAssocArrayLookup(&arr2, str3, strlen(str3), (void **)&h);
  sbAssocArrayLookup(&arr2, &b, sizeof(b), (void **)&str4);
*/
  sbStringCreate(&string, "Hello, ", 1);
  sbStringCreate(&string2, str3, 1);
  sbStringCreate(&string3, "<q>", 1);

  //sbArrayCreate(&arr,0);

  sbStringSplit(&string2, &string3, -1, &arr);

  for( int i=0; i < sbArrayCount(&arr); i++)
  {
    SBString *s;
    sbArrayElemAt(&arr, i, (void **)&s);
    printf("%.*s/", sbStringCharWidth(s)*sbStringLength(s), s->data);
  }

  printf("\n");
//  printf("'%.*s' '%.*s'\n", sbStringCharWidth(&string)*sbStringLength(&string), string.data, sbStringCharWidth(&string2) * sbStringLength(&string2), string2.data);
/*
  sbStringConcat(&string, &string2);

  printf("%.*s\n", sbStringCharWidth(&string)*sbStringLength(&string), string.data);

  printf("%d %s %d\n", *d, str2, *e);
  printf("%d %d %s\n", *g, *h, str4);
*/
//  sbAssocArrayDelete(&arr2);
  sbArrayDelete(&arr);
  sbStringDelete(&string);
  sbStringDelete(&string2);
  sbStringDelete(&string3);
  return 0;
}
