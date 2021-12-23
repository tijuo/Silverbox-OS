/* The Block Cache */

#include <kernel/device.h>
#include <kernel/bcache.h>
#include <kernel/semaphore.h>

#define MAX_BUFF         1024
#define NUM_SETS	 128
#define ENTRIES_PER_SET  8

#define CALC_SET(blk, dev) ((blk * dev) % NUM_SETS)

// 8 way set-associative

struct Block blkCache[NUM_SETS][ENTRIES_PER_SET];
struct Block *bfind(int devnum, unsigned long blk);

unsigned counter=0;

/* Operations of the block cache */

/* ReadBlock(): Read data from a cached block
   WriteBlock(): Write data to a cached block
   SyncBlock(): Write cached block to disk
*/

/* Helper functions:

   LockBlock():
   UnlockBlock():
   ReplaceBlock(): Take out least used block and replace it with a new one
   FindBlock(): Attempt to locate a block in the cache
   LoadBlock(): Copy a block from a device to the cache
*/


int findBlock( unsigned blockNum, int devNum, int set )
{
  struct Block *blk = &blkCache[set][0];
  int i;

  for( i=0; i < ENTRIES_PER_SET; i++, blk++ )
  {
    if( blk->blockNum == blockNum && blk->devNum == devNum
        && blk->data != NULL )
    {
      return i;
    }
  }

  return -1;
}

unsigned findLRU_Block( int devNum, int set )
{
  struct Block *blk = &blkCache[set][1];
  int i, diff = counter - blkCache[set][0].useCount;
  unsigned blkNum = 0;

  for( i=1; i < ENTRIES_PER_SET; i++, blk++ )
  {
    if( counter - blkCache[set][i].useCount > diff )
    {
      diff = counter - blkCache[set][i].useCount;
      blkNum = i;
    }
  }

  return blkNum;
}

int cacheRead( int devNum, unsigned offset, size_t blocks, void *buffer )
{
  int setEntry;
  struct block *blk;

  if( buffer == NULL )
    return -1;

  for( int i=0; i < blocks; i++ )
  {
    setEntry = findBlock( devNum, CALC_SET( offset + i, devNum ) );

//    if( setEntry < 0 )
//      LoadBlock();

    blk = &blkCache[CALC_SET( offset + i, devNum )][setEntry];

    memcpy( buffer + i * blk->blkSize, blk->data, blk->blkSize );
    blk->accessed = 1;
    blk->useCount = counter++;
  }
}

int cacheWrite( int devNum, unsigned offset, size_t blocks, void *buffer )
{
  for( int i=0; i < blocks; i++ )
  {
    setEntry = findBlock( devNum, CALC_SET( offset + i, devNum ) );

//    if( setEntry < 0 )
//      LoadBlock();

    blk = &blkCache[CALC_SET( offset + i, devNum )][setEntry];

    memcpy( blk->data, buffer + i * blk->blkSize, blk->blkSize );
    blk->accessed = 1;
    blk->dirty = 1;
    blk->useCount = counter++;
  }

}

void *loadBlock( int devNum, unsigned blkNum )
{
  int blkSize;
  void *buffer;

  blkSize = deviceIoctl( IOCTL_GET_BLKSIZE, 1, devNum );

  if( blkSize < 0 )
    return NULL;

  buffer = malloc( blkSize );

  if( buffer == NULL )
    return NULL;

//  deviceRead(  );
}

/* Implementation of block() and bunlock() is broken. */

int bload(struct buffer *blk)
{
  return do_request(blk->device, blk->block, 1, REQ_READ, blk->data, NULL);
}

struct buffer *bnew(int devnum, unsigned long blk)
{
  int i;

  for(i=0; i < MAX_BUFF; i++)
  {
    if(blist[i].device == 0) {
      blist[i].device = devnum;
      blist[i].block = blk;
      blist[i].lock = 1;
      blist[i].dirty = 0;
      blist[i].data = (void *)kmalloc(blk_dev[MAJOR(devnum)].blk_size);
      if(blist[i].data == NULL) {
        blist[i].device = 0;
        return NULL;
      }
      return &blist[i];
    }
  }
  return NULL;
}

int bdelete(int devnum, unsigned long blk)
{
  struct buffer *b;

  b = bfind(devnum, blk);
  if(b == NULL)
    return -1;

  kfree(b->data);
  b->device = 0;
  return 0;
}

struct buffer *bfind(int devnum, unsigned long blk)
{
  int i;

  for(i=0; i < MAX_BUFF; i++) {
    if((blist[i].device == devnum) && (blist[i].block == blk))
      return &blist[i];
  }
  return NULL;
}

void block(struct buffer *blk)
{
  MutexWait(&blk->lock);
}

void bunlock(struct buffer *blk)
{
  MutexSignal(&blk->lock);
}

struct buffer *bread(int devnum, unsigned long blknum)
{
  struct buffer *b;
  b = (struct buffer *)bfind(devnum, blknum);
  if(b == NULL)
    return NULL;
  block(b);
  bload(b);
  bunlock(b);
  return b;
}

