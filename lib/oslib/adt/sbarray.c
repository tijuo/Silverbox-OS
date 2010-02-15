#include "sbarray.h"
#include <stdarg.h>

static void shiftArray(void **ptr, int nElems, int shift);
int sbArrayClear(SBArray *);
SBArray *sbArrayCreate(int numElems, ...);
int sbArrayElemAt(SBArray *array, int pos, void **elem);
int sbArrayFind(SBArray *array, void *elem);
int sbArrayInsert(SBArray *array, void *ptr, int pos);
int sbArrayLength(SBArray *array);
int sbArrayPop(SBArray *array, void **ptr);
int sbArrayPush(SBArray *array, void *ptr);
int sbArrayRemove(SBArray *array, int pos);

static void shiftArray(int **ptr, int nElems, int shift)
{
  if( shift > 0 )
  {
    for(int i=nElems; i > 0; i--)
      ptr[i+shift-1] = ptr[i-1];
  }
  else if( shift < 0 )
  {
    for(int i=0; i < nElems; i++)
      ptr[i+shift] = ptr[i];
  }
}

static int adjustArrayCapacity(SBArray *array)
{
  void *result;

  if( array->nElems == array->capacity )
  {
    array->capacity = (int)(array->nElems * 1.5);
    result = realloc(array->ptrs, array->capacity * sizeof(int *));

    if( !result )
      return -1;
  }
  else if( (int)(array->nElems * 1.6) < array->capacity )
  {
    array->capacity = array->nElems + 3;
    result = realloc(array->ptrs, array->capacity * sizeof(int *));

    if( !result )
      return -1;
  }

  return 0;
}

int sbArrayClear(SBArray *array)
{
  if( !array )
    return SBArrayError;
  
  array->nElems = 0;
  adjustArrayCapacity(array);

  return 0;
}

int sbArrayCopy(const SBArray *array, SBArray *newArray)
{
  SBArray *newArray;

  if( !array || !newArray )
    return SBArrayError;

  newArray->ptrs = (int **)malloc(array->capacity * sizeof(int *));

  if( !newArray->ptrs )
  {
    newArray->capacity = newArray->nElems = 0;
    return SBArrayFailed;
  }

  for(int i=0; i < array->nElems; i++)
    memcpy(newArray->ptrs, array->ptrs, array->nElems * sizeof(int *));

  newArray->capacity = array->capacity;
  newArray->nElems = array->nElems;

  return 0;
}

int sbArrayCreate(SBArray *array, int numElems, ...)
{
  va_args args;

  if( !array || numElems < 0 )
    return SBArrayError;

  va_start(args);

  array->nElems = 0;
  array->capacity = numElems + 2 > (int)(numElems * 1.5) ? 
                       numElems + 2 : (int)(numElems * 1.5);
  array->ptrs = (int **)malloc(array->capacity * sizeof(int *));

  if( array->ptrs == NULL )
  {
    array->capacity = array->nElems = 0;
    return SBArrayFailed;
  }

  for(int i=0; i < numElems; i++)
  {
    array->ptrs[i] = va_arg(args, int *);
    array->nElems++;
  }

  va_end(args);

  return 0;
}

int sbArrayDelete(SBArray *array)
{
  if( !array )
    return SBArrayError;

  free(array->ptrs);
  free(array);
  array->capacity = 0;
  array->nElems = 0;

  return 0;
}

int sbArrayElemAt(const SBArray *array, int pos, void **elem)
{
  if( !array )
    return SBArrayError;

  if( pos >= nElems || pos < 0 )
    return SBArrayNotFound;

  if( elem )
    *(int **)elem = array->ptrs[pos];

  return 0;
}

int sbArrayFind(const SBArray *array, void *elem)
{
  if( !array )
    return SBArrayError;

  for(int i=0; i < array->nElems; i++)
  {
    if( array->ptrs[i] == (int *)elem )
      return i;
  }

  return SBArrayNotFound;
}

int sbArrayInsert(SBArray *array, void *ptr, int pos)
{
  int newSize;

  if( !array || pos < 0 )
    return SBArrayError;

  newSize = (pos >= array->nElems ? pos : array->nElems) + 1;

  if( newSize >= array->capacity )
  {
    array->capacity = (int)(newSize * 1.5);
    realloc(array->ptrs, array->capacity * sizeof(int *));
  }

  if( pos >= array->nElems )
  {
    for( int i=array->nElems; i < pos; i++ )
      array->ptrs[i] = NULL;

    array->ptrs[pos] = (int *)ptr;
    array->nElems = pos + 1;
  }
  else
  {
    shiftArray( array->ptrs + pos, array->nElems - pos, 1 );
    array->ptrs[pos] = (int *)ptr;
    array->nElems++;
  }

  return 0;
}

int sbArrayLength(const SBArray *array)
{
  if( !array )
    return SBArrayError;
  else
    return array->nElems;
}

int sbArrayPop(SBArray *array, void **ptr)
{
  if( !array )
    return SBArrayError;

  if( array->nElems == 0 )
    return SBArrayEmpty;
  else if( ptr )
    *(int **)ptr = array->ptrs[array->nElems-1];

  array->nElems--;

  adjustArrayCapacity(array);

  return 0;
}

int sbArrayPush(SBArray *array, void *ptr)
{
  if( !array )
    return SBArrayError;

  if( adjustArrayCapacity(array) < 0 )
    return SBArrayFailed;

  if( ptr )
    array->ptrs[array->nElems] = (int *)ptr;
  else
    array->ptrs[array->nElems] = NULL;

  array->nElems++;

  return 0;
}

int sbArrayRemove(SBArray *array, int pos)
{
  if( !array )
    return SBArrayError;

  if( pos >= array->nElems || pos < 0 )
    return SBArrayNotFound;

  shiftArray( array->ptrs + pos + 1, array->nElems - pos - 1, -1 );
  array->nElems--;

  adjustArrayCapacity(array);

  return 0;
}

int sbArraySlice(const SBArray *array, int start, int end, SBArray *newArray)
{
  if( !array || !newArray )
    return SBArrayError;

  if( start < 0 )
    start = 0;

  if( end > array->nElems )
    end = array->nElems;

  newArray->ptrs = NULL;
  newArray->capacity = 0;

  if( end - start <= 0 )
  {
    newArray->nElems = 0;
    adjustArrayCapacity( newArray );
  }
  else
  {
    newArray->nElems = end - start;

    if( adjustArrayCapacity( newArray ) < 0 )
    {
      newArray->capacity = 0;
      return SBArrayFailed;
    }

    for( int i=start; i < end; i++ )
      sbArrayPush(newArray, (void *)array->ptrs[i]);
  }

  return 0;
}
