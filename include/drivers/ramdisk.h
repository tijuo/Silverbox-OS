#ifndef RAMDISK
#define RAMDISK

#define RAMDISK_MAJOR       10
#define NUM_RAMDISKS        5
#define RAMDISK_SIZE        (1024 * 1440)
#define RAMDISK_BLKSIZE     512

struct Ramdisk {
  int devNum;
  unsigned char *data;
  unsigned blockSize;
  unsigned numBlocks;
} ramdiskDev[NUM_RAMDISKS];

int readRamdisk( int device, size_t numBlocks, unsigned offset, void *buffer );
int writeRamdisk( int device, size_t numBlocks, unsigned offset, void *buffer );


#endif
