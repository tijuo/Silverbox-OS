#include <os/ostypes/sbarray.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

static void shiftArray(void **ptr, int nElems, int shift);
static int adjustArrayCapacity(sbarray_t *array);
static int getPosition(sbarray_t *array, int pos);

static int getPosition(sbarray_t *array, int pos)
{
  if(pos == POS_START)
    pos = 0;
  else if(pos == POS_END)
    pos = array->nElems;

  return pos;
}

static void shiftArray(void **ptr, int nElems, int shift)
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

static int adjustArrayCapacity(sbarray_t *array)
{
  void **result;
  int prev_capacity = array->capacity;

  if( array->nElems == array->capacity )
    array->capacity = (3 * array->nElems) / 2;
  else
    array->capacity = array->nElems + 3;

  result = realloc(array->elems, array->capacity * sizeof(void *));

  if( !result )
  {
    array->capacity = prev_capacity;
    return SB_FAIL;
  }

  array->elems = result;

  return 0;
}

int sbArrayClear(sbarray_t *array)
{
  if( !array )
    return SB_FAIL;

  array->nElems = 0;
  adjustArrayCapacity(array);

  return 0;
}

sbarray_t *sbArrayCopy(sbarray_t *array, sbarray_t *newArray)
{
  if( !array || !newArray )
    return NULL;

  newArray->elems = (void **)malloc(array->capacity * sizeof(void *));

  if( !newArray->elems )
  {
    newArray->capacity = newArray->nElems = 0;
    return NULL;
  }

  memcpy(newArray->elems, array->elems, array->nElems * sizeof(void *));

  newArray->capacity = array->capacity;
  newArray->nElems = array->nElems;

  return newArray;
}

size_t sbArrayCount(sbarray_t *array)
{
  return !array ? 0 : (size_t)array->nElems;
}

int sbArrayCreate(sbarray_t *array)
{
  if( !array )
    return SB_FAIL;

  array->nElems = 0;
  array->capacity = 5;

  array->elems = (void **)malloc(array->capacity * sizeof(void *));

  if( !array->elems )
  {
    array->capacity = array->nElems = 0;
    return SB_FAIL;
  }

  return 0;
}

void sbArrayDelete(sbarray_t *array)
{
  if( !array )
    return;

  free(array->elems);

  array->elems = NULL;
  array->capacity = 0;
  array->nElems = 0;
}

int sbArrayGet(sbarray_t *array, int pos, void **elem)
{
  if( !array )
    return SB_FAIL;

  pos = getPosition(array, pos);

  if( pos >= array->nElems || pos < 0 )
    return SB_NOT_FOUND;

  if( elem )
    *elem = (void *)array->elems[pos];

  return 0;
}

// Returns the index of the first occurence of elem (if found)

int sbArrayFind(sbarray_t *array, void *elem)
{
  if(!array)
    return SB_FAIL;

  for(int i=0; i < array->nElems; i++)
  {
    if(array->elems[i] == elem)
      return (int)i;
  }

  return SB_NOT_FOUND;
}

// Insert an element into a location in the array.

int sbArrayInsert(sbarray_t *array, int pos, void *ptr)
{
  int newSize;

  pos = getPosition(array, pos);

  if(!array || pos < 0 || pos > array->nElems || array->nElems == INT_MAX)
    return SB_FAIL;

  newSize = (pos >= array->nElems ? pos : array->nElems) + 1;

  if( newSize >= array->capacity )
  {
    void **newElems;

    array->capacity = (3*newSize)/2;
    newElems = (void **)realloc(array->elems, array->capacity * sizeof(void *));

    if(!newElems)
      return SB_FAIL;

    array->elems = newElems;
  }

  shiftArray( array->elems + pos, array->nElems - pos, 1 );

  array->elems[pos] = ptr;
  array->nElems++;

  return 0;
}

// Pops an element from the end of the array

int sbArrayPop(sbarray_t *array, void **ptr)
{
  if( !array )
    return SB_FAIL;
  else if( !array->nElems )
    return SB_EMPTY;

  if( ptr )
    *ptr = (void **)array->elems[--array->nElems];

  adjustArrayCapacity(array);

  return 0;
}

// Pushes an element to the end of the array

int sbArrayPush(sbarray_t *array, void *ptr)
{
  if(!array || !ptr || array->nElems == INT_MAX || adjustArrayCapacity(array) < 0)
    return SB_FAIL;

  array->elems[array->nElems++] = ptr;
  return 0;
}

int sbArrayRemove(sbarray_t *array, int pos)
{
  if( !array )
    return SB_FAIL;

  pos = getPosition(array, pos);

  if( pos >= array->nElems || pos < 0)
    return SB_NOT_FOUND;

  shiftArray( array->elems + pos + 1, array->nElems - pos - 1, -1 );
  array->nElems--;
  adjustArrayCapacity(array);

  return 0;
}

/* Returns the subarray in an array between positions 'start' (inclusive)
   and 'end' (non-inclusive) */

int sbArraySlice(sbarray_t *array, int start, int end, sbarray_t *newArray)
{
  if( !array || !newArray )
    return SB_FAIL;

  start = getPosition(array, start);
  end = getPosition(array, end);

  if( start < 0 )
    start = 0;

  if( end > array->nElems )
    end = array->nElems;

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
      return SB_FAIL;
    }

    for( int i=start; i < end; i++ )
    {
      if(sbArrayPush(newArray, array->elems[i]) != 0)
      {
        sbArrayDelete(newArray);
        return SB_FAIL;
      }
    }
  }

  return 0;
}
