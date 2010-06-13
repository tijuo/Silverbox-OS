#include <os/ostypes/sbarray.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

static void shiftArray(struct _SBArrayElem *ptr, int nElems, int shift);
static int adjustArrayCapacity(SBArray *array);

static void shiftArray(struct _SBArrayElem *ptr, int nElems, int shift)
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
  struct _SBArrayElem *result;

  if( array->nElems == array->capacity )
  {
    array->capacity = (int)(array->nElems * 1.5);
    result = realloc(array->elems, array->capacity * sizeof(struct _SBArrayElem));

    if( !result )
      return -1;

    array->elems = result;
  }
  else if( (int)(array->nElems * 1.6) < array->capacity )
  {
    array->capacity = array->nElems + 3;
    result = realloc(array->elems, array->capacity * sizeof(struct _SBArrayElem));

    if( !result )
      return -1;

    array->elems = result;
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
  if( !array || !newArray )
    return SBArrayError;

  newArray->elems = (struct _SBArrayElem *)malloc(array->capacity * 
    sizeof(struct _SBArrayElem));

  if( !newArray->elems )
  {
    newArray->capacity = newArray->nElems = 0;
    return SBArrayFailed;
  }

  newArray->nElems = 0;

  for(unsigned i=0; i < (unsigned)array->nElems; i++, newArray->nElems++)
  {
    newArray->elems[i].size = array->elems[i].size;
    newArray->elems[i].ptr = malloc(array->elems[i].size);

    if( !newArray->elems[i].ptr )
    {
      sbArrayDelete(newArray);
      return SBArrayFailed;
    }

    memcpy(newArray->elems[i].ptr, array->elems[i].ptr, array->elems[i].size);
  }

  newArray->capacity = array->capacity;

  return 0;
}

int sbArrayCount(const SBArray *array)
{
  if( !array )
    return SBArrayError;
  else
    return array->nElems;
}

int sbArrayCreate(SBArray *array)
{
  if( !array )
    return SBArrayError;

  array->nElems = 0;
  array->capacity = 5;

  array->elems = (struct _SBArrayElem *)malloc(array->capacity *
     sizeof(struct _SBArrayElem));

  if( array->elems == NULL )
  {
    array->capacity = array->nElems = 0;
    return SBArrayFailed;
  }

  return 0;
}

int sbArrayDelete(SBArray *array)
{
  if( !array )
    return SBArrayError;

  for(unsigned i=0; i < (unsigned)array->nElems; i++)
    free(array->elems[i].ptr);

  free(array->elems);

  array->elems = NULL;
  array->capacity = 0;
  array->nElems = 0;

  return 0;
}

int sbArrayElemAt(const SBArray *array, int pos, void **elem, size_t *size)
{
  if( !array )
    return SBArrayError;

  if( pos >= array->nElems || pos < 0 )
    return SBArrayNotFound;

  if( elem )
    *(int **)elem = (int *)array->elems[pos].ptr;

  if( size )
    *size = array->elems[pos].size;

  return 0;
}

// Returns the index of the first occurence of elem (if found)

int sbArrayFind(const SBArray *array, void *elem, size_t size)
{
  if( !array || size == 0 )
    return SBArrayError;

  for(unsigned i=0; i < (unsigned)array->nElems; i++)
  {
    if( size == array->elems[i].size && 
        memcmp(array->elems[i].ptr, elem, size) == 0 )
    {
      return (int)i;
    }
  }

  return SBArrayNotFound;
}

// Insert an element into a location in the array.

int sbArrayInsert(SBArray *array, int pos, void *ptr, size_t size)
{
  int newSize;

  if( !array || pos < 0 || size == 0 )
    return SBArrayError;

  if( pos > array->nElems )
    pos = array->nElems;

  newSize = (pos == array->nElems ? pos : array->nElems) + 1;

  if( newSize >= array->capacity )
  {
    struct _SBArrayElem *newElems;

    array->capacity = (int)(newSize * 1.5);
    newElems = realloc(array->elems, array->capacity * 
       sizeof(struct _SBArrayElem));

    if( newElems == NULL )
      return SBArrayFailed;

    array->elems = newElems;
  }

  if( pos == array->nElems )
  {
    array->elems[pos].ptr = malloc(size);

    if( !array->elems[pos].ptr )
      return SBArrayFailed;

    memcpy(array->elems[pos].ptr, ptr, size);
    array->elems[pos].size = size;
    array->nElems = pos + 1;
  }
  else
  {
    shiftArray( array->elems + pos, array->nElems - pos, 1 );

    array->elems[pos].ptr = malloc(size);

    if( !array->elems[pos].ptr )
      return SBArrayFailed;

    memcpy(array->elems[pos].ptr, ptr, size);
    array->elems[pos].size = size;
    array->nElems++;
  }

  return 0;
}

// Pops an element from the end of the array

int sbArrayPop(SBArray *array, void **ptr, size_t *size)
{
  if( !array )
    return SBArrayError;

  if( array->nElems == 0 )
    return SBArrayEmpty;

  if( ptr )
    *(int **)ptr = (int *)array->elems[array->nElems-1].ptr;
  else
    free(array->elems[array->nElems-1].ptr);

  if( size )
    *size = array->elems[array->nElems-1].size;

  array->nElems--;

  adjustArrayCapacity(array);

  return 0;
}

// Pushes an element to the end of the array

int sbArrayPush(SBArray *array, void *ptr, size_t size)
{
  if( !array || !ptr || size == 0 )
    return SBArrayError;

  if( adjustArrayCapacity(array) < 0 )
    return SBArrayFailed;

  array->elems[array->nElems].ptr = malloc(size);

  if( !array->elems[array->nElems].ptr )
    return SBArrayFailed;

  array->elems[array->nElems].size = size;

  memcpy(array->elems[array->nElems].ptr, ptr, size);

  array->nElems++;

  return 0;
}

int sbArrayRemove(SBArray *array, int pos)
{
  if( !array )
    return SBArrayError;

  if( pos >= array->nElems || pos < 0 )
    return SBArrayNotFound;

  free(array->elems[pos].ptr);

  shiftArray( array->elems + pos + 1, array->nElems - pos - 1, -1 );
  array->nElems--;

  adjustArrayCapacity(array);

  return 0;
}

/* Returns the subarray in an array between positions 'start' (inclusive) 
   and 'end' (non-inclusive) */

int sbArraySlice(const SBArray *array, int start, int end, SBArray *newArray)
{
  if( !array || !newArray )
    return SBArrayError;

  if( start < 0 )
    start = 0;

  if( end > array->nElems )
    end = array->nElems;

//  sbArrayDelete(newArray);

  newArray->elems = NULL;
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
      sbArrayPush(newArray, (void *)array->elems[i].ptr, array->elems[i].size);
  }

  return 0;
}
