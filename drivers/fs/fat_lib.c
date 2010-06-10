#include "os/fatfs.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <os/vfs.h>
#include <os/dev_interface.h>
#include <os/services.h>

struct FatDir
{
  struct FAT_DirEntry *entries; // the sub-entries (files, dirs) in this directory
  struct FAT_Dev *fatDev;

  size_t num_entries;	// the number of sub-entries

  union
  {
    unsigned cluster;
    unsigned sector;
  };

  unsigned next;	// the next sector/cluster from which to read the entries

  char more : 1;	// indicates that there are more entries to be read
  char use_sects : 1;	// read from sectors instead of clusters
};

/* TODO: Need to implement LFNs and FAT32 */

/* XXX: readCluster() and writeCluster() are broken! 
   If the size of a block isn't divisible by 512, then there's
   a big problem. If the size of a block is greater than 512,
   there's a problem. */

extern int writeFAT(struct FAT_Dev *fatDev, dword entry, unsigned cluster);
extern unsigned readFAT(struct FAT_Dev *fatDev, unsigned cluster);
extern unsigned getFreeCluster(struct FAT_Dev *fatDev);

static int checkFAT_Char( char c );
static size_t get_token_len( const char *str );
static char *generate8_3Name( const char *name, size_t nameLen, char *new_name );
static int calcRootDirSecs(union FAT_BPB *bpb);
static int firstDataSec(union FAT_BPB *bpb);
static int calcFirstSecClus(int clusnum, union FAT_BPB *bpb);
static int calcDataSecs(union FAT_BPB *bpb);
static int calcClusCount(union FAT_BPB *bpb);
static enum FAT_Type determineFAT_Type(union FAT_BPB *bpb);
static int calcFatSize(union FAT_BPB *bpb);
static int calcClusterSize( union FAT_BPB *bpb );
static enum FAT_ClusterType getClusterType( unsigned cluster, enum FAT_Type type );
static int getDeviceData( unsigned short devNum, struct FAT_Dev *fatDev );
int readSector( struct FAT_Dev *fatDev, unsigned sector, void *buffer );
static int readCluster( struct FAT_Dev *fatDev, unsigned cluster, void *buffer );
static int getFAT_DirEntry( const char *path, struct FAT_Dev *fatDev, 
                            struct FAT_DirEntry *dirEntry );
static int _createEntry( const char *path, const char *name, 
                         struct FAT_Dev *fat_dev, bool dir );
static struct FatDir *readDirEntry( const char *path, struct FAT_Dev *fat_dev );
static struct FatDir *readNextEntry( struct FatDir *fat_dir );
static void freeFatDir( struct FatDir *fat_dir );
int writeSector( struct FAT_Dev *fatDev, unsigned sector, void *buffer );
int fatReadFile( const char *path, unsigned int offset, unsigned short devNum,
                 char *fileBuffer, size_t length );
int fatGetDirList( const char *path, unsigned short devNum, struct FileAttributes *attrib,
                   size_t maxEntries );
int fatGetAttributes( const char *path, unsigned short devNum, struct FileAttributes *attrib );
int fatCreateFile( const char *path, const char *name, unsigned short devNum );
int fatGetAttributes( const char *path, unsigned short devNum, struct FileAttributes *attrib );
int fatCreateDir( const char *path, const char *name, unsigned short devNum );

/* Indicates whether a character is valid for a FAT file/directory.
   Returns 0 on a valid char. -1 for invalid char. */

static int checkFAT_Char( char c )
{
  char *badChars = "\"*+,./:;<=>?[\\]|";

  while( *badChars && *badChars++ != c );

  return (c == '|' || *badChars) ? -1 : 0;;
}

/* Converts an unformatted 8.3 filename into proper form for a FAT file entry name. 
   name is a familiar filename (like "somefile.txt")
   nameLen is the length of the name
   Returns new_name which is a 11 byte buffer. */

static char *generate8_3Name( const char *name, size_t nameLen, char *new_name )
{
  char filename[11];
  int i;

  if( name == NULL || new_name == NULL || nameLen == 0 || name[0] == '\0')
    return NULL;

  memset(filename, ' ', 11);

  if( nameLen == 1 && name[0] == '.' )
    filename[0] = '.';
  else if( nameLen == 2 && name[0] == '.' )
  {
    if( name[1] == '.' )
      filename[0] = filename[1] = '.';
    else
      return NULL;
  }
  else
  {
    unsigned dotIndex;

    for(dotIndex=0; dotIndex < nameLen; dotIndex++ )
    {
      if( name[dotIndex] ==  '.' )
        break;
      else if( checkFAT_Char( name[dotIndex] ) < 0 )
        return NULL;
    }

    if( dotIndex > 8 )
      return NULL;

    memcpy( filename, name, dotIndex );

    if( dotIndex != nameLen )// if we haven't found the dot
      memcpy( &filename[8], &name[dotIndex+1], nameLen-dotIndex-1 );
  }

  for(i=0; i < 11; i++)
    new_name[i] = (islower(filename[i]) ? toupper(filename[i]) : filename[i]);

  return new_name;
}

/* Returns the length of the first token in a path name.
   A token may be the name of a file or directory.
   ex. "tfiles/fil1.txt" returns length 6 */

static size_t get_token_len( const char *str )
{
  const char * const s = str;

  if( str == NULL )
    return 0;

  while( *str != '\0' && *str != '/')
    str++;

  return (size_t)(str - s);
}

/* Calculates the number of sectors allocated for the root directory */

static int calcRootDirSecs(union FAT_BPB *bpb)
{
  int rootdirs, type;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
    rootdirs = bpb->fat12.root_ents;
  else if(type == FAT16)
    rootdirs = bpb->fat16.root_ents;

  return (rootdirs * 32) / FAT_SECTOR_SIZE;
}

/* Returns the first data sector after the BPB, FAT, and
   Root Dirs */

static int firstDataSec(union FAT_BPB *bpb)
{
  int fatsize, resd, copies, type;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
  {
    fatsize = bpb->fat12.secs_per_fat;
    resd = bpb->fat12.resd_secs;
    copies = bpb->fat12.fat_copies;
  }
  else if(type == FAT16)
  {
    fatsize = bpb->fat16.secs_per_fat;
    resd = bpb->fat16.resd_secs;
    copies = bpb->fat16.fat_copies;
  }

  if(fatsize == 0)
    fatsize = bpb->fat32.secs_per_fat;

  return resd + (copies * fatsize) + calcRootDirSecs(bpb);
}

/* Returns the first sector of the cluster provided.
     (Data Clusters only!) */

static int calcFirstSecClus(int clusnum, union FAT_BPB *bpb)
{
  int size, type;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
    size = bpb->fat12.secs_per_clus;
  else if(type == FAT16)
    size = bpb->fat16.secs_per_clus;

  if(size == 0)
    size = bpb->fat32.secs_per_clus;

  return ((clusnum - 2) * size) +  (firstDataSec(bpb) / size);
}

static int calcDataSecs(union FAT_BPB *bpb)
{
  int fatsize, total, resd, copies, type;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
  {
    fatsize = bpb->fat12.secs_per_fat;
    total = bpb->fat12.total_secs;
    resd = bpb->fat12.resd_secs;
    copies = bpb->fat12.fat_copies;
  }
  else if(type == FAT16)
  {
    fatsize = bpb->fat16.secs_per_fat;
    total = bpb->fat16.total_secs;
    resd = bpb->fat16.resd_secs;
    copies = bpb->fat16.fat_copies;
  }

  if(fatsize == 0)
    fatsize = bpb->fat32.secs_per_fat;

  if(total == 0)
    total = bpb->fat32.secs_in_fs;

  return total - resd + (copies * fatsize) + calcRootDirSecs(bpb);
}

/* Calculate the amount of clusters that are present on the disk.
   (Data Clusters only!) */

static int calcClusCount(union FAT_BPB *bpb)
{
  int secs_per_clus, type;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
    secs_per_clus = bpb->fat12.secs_per_clus;
  else if(type == FAT16)
    secs_per_clus = bpb->fat16.secs_per_clus;

  return calcDataSecs(bpb) / secs_per_clus;
}

static enum FAT_Type determineFAT_Type(union FAT_BPB *bpb)
{
  unsigned count = bpb->fat12.total_secs;

  if(count <= 4086)
    return FAT12;
  else if(count <= 65526)
    return FAT16;
  else
    return FAT32;
}

/* Find the the size of one FAT in sectors.
   !!! Broken. !!! */

static int calcFatSize(union FAT_BPB *bpb)
{
  int totalsecs, resdsecs, rootents, clussize;
  float result;
  enum FAT_Type type;
  float fat_len;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
  {
    clussize = bpb->fat12.secs_per_clus;
    resdsecs = bpb->fat12.resd_secs;
    rootents = bpb->fat12.root_ents;
    totalsecs = bpb->fat12.total_secs;
    fat_len = 1.5;
  }
  else if(type == FAT16)
  {
    clussize = bpb->fat16.secs_per_clus;
    resdsecs = bpb->fat16.resd_secs;
    rootents = bpb->fat16.root_ents;
    totalsecs = bpb->fat16.total_secs;
    fat_len = 2;
  }

  result = (fat_len * (totalsecs - resdsecs - ((rootents * 32) / FAT_SECTOR_SIZE))) / \
	(clussize * FAT_SECTOR_SIZE + 4);

  if(result != (int)result)
    result = (int)result + 1;

  return (int)result;
}

static int calcClusterSize( union FAT_BPB *bpb )
{
  enum FAT_Type type;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
    return bpb->fat12.secs_per_clus * FAT_SECTOR_SIZE;
  else if(type == FAT16)
    return bpb->fat16.secs_per_clus * FAT_SECTOR_SIZE;

  return -1;
}

static enum FAT_ClusterType getClusterType( unsigned cluster, enum FAT_Type type )
{
  switch( type )
  {
    case FAT12:
      if( cluster == 0 )
        return FREE_CLUSTER;
      else if( cluster >= 0xFF8 && cluster <= 0xFFF )
        return END_CLUSTER;
      else if( cluster >= 2 && cluster <= 0xFEF )
        return USED_CLUSTER;
      else if( cluster == 0xFF7 )
        return BAD_CLUSTER;
      else if( cluster == 1 || (cluster >= 0xFF0 && cluster <= 0xFF6) )
        return RESD_CLUSTER;
      break;
    case FAT16:
      if( cluster == 0 )
        return FREE_CLUSTER;
      else if( cluster >= 0xFFF8 && cluster <= 0xFFFF )
        return END_CLUSTER;
      else if( cluster >= 2 && cluster <= 0xFFEF )
        return USED_CLUSTER;
      else if( cluster == 0xFFF7 )
        return BAD_CLUSTER;
      else if( cluster == 1 || (cluster >= 0xFFF0 && cluster <= 0xFFF6) )
        return RESD_CLUSTER;
      break;
    case FAT32:
      cluster &= 0xFFFFFFF;

      if( cluster == 0 )
        return FREE_CLUSTER;
      else if( cluster >= 0xFFFFFF8 && cluster <= 0xFFFFFFF )
        return END_CLUSTER;
      else if( cluster >= 2 && cluster <= 0xFFFFFEF )
        return USED_CLUSTER;
      else if( cluster == 0xFFFFFF7 )
        return BAD_CLUSTER;
      else if( cluster == 1 || (cluster >= 0xFFFFFF0 && cluster <= 0xFFFFFF6) )
        return RESD_CLUSTER;
    default:
      return INVALID_CLUSTER;
  }
  return INVALID_CLUSTER;
}

/* This reads in an entire cluster from a device into a buffer.

 XXX: This implementation was just slapped together and needs work.
   Buffer must be at LEAST (bytesPerSector * sectorsPerCluster) long. It has to be able
   to hold an entire cluster. */

static int readCluster( struct FAT_Dev *fatDev, unsigned cluster, void *buffer )
{
  unsigned blocksPerSector, sectorsPerCluster;
  unsigned blockStart;
  enum FAT_ClusterType clusterType;

  if( fatDev == NULL || buffer == NULL )
    return -1;

  clusterType = getClusterType( cluster, fatDev->cache.fatType );

  /* Why would you ever want to read a free cluster? */

  if( clusterType != USED_CLUSTER && clusterType != FREE_CLUSTER )
    return -1;

  if(fatDev->cache.fatType == FAT12)
    sectorsPerCluster = fatDev->bpb.fat12.secs_per_clus;
  else if(fatDev->cache.fatType == FAT16)
    sectorsPerCluster = fatDev->bpb.fat16.secs_per_clus;
  else
    return -1; // unsupported

  blocksPerSector = FAT_SECTOR_SIZE / fatDev->device.dataBlkLen; // FIXME

  blockStart = (fatDev->cache.firstDataSec + (cluster-2) * sectorsPerCluster) * blocksPerSector;

  if( deviceRead( fatDev->device.ownerTID, MINOR(fatDev->deviceNum), blockStart, blocksPerSector * 
                  sectorsPerCluster, FAT_SECTOR_SIZE * sectorsPerCluster, buffer ) == -1 )
  {
    return -1;
  }
  return 0;
}

/* Buffer should be capable of holding 512 bytes (an entire sector). */

int readSector( struct FAT_Dev *fatDev, unsigned sector, void *buffer )
{
  unsigned blocksPerSector;
  unsigned blockStart;

  if( fatDev == NULL || buffer == NULL )
    return -1;

  blocksPerSector = FAT_SECTOR_SIZE / fatDev->device.dataBlkLen; // FIXME

  blockStart = sector * blocksPerSector;

  if( deviceRead( fatDev->device.ownerTID, MINOR(fatDev->deviceNum), blockStart, blocksPerSector,
                  FAT_SECTOR_SIZE, buffer ) == -1 )
  {
    return -1;
  }
  return 0;
}

static int writeCluster( struct FAT_Dev *fatDev, unsigned cluster, void *buffer )
{
  unsigned blocksPerSector, sectorsPerCluster;
  unsigned blockStart;
  enum FAT_ClusterType clusterType;

  if( fatDev == NULL || buffer == NULL )
    return -1;

  clusterType = getClusterType( cluster, fatDev->cache.fatType );

  if( clusterType != USED_CLUSTER && clusterType != FREE_CLUSTER )
    return -1;

  if(fatDev->cache.fatType == FAT12)
    sectorsPerCluster = fatDev->bpb.fat12.secs_per_clus;
  else if(fatDev->cache.fatType == FAT16)
    sectorsPerCluster = fatDev->bpb.fat16.secs_per_clus;
  else
    return -1; // unsupported

  blocksPerSector = FAT_SECTOR_SIZE / fatDev->device.dataBlkLen; // FIXME

  blockStart = (fatDev->cache.firstDataSec + (cluster-2) * sectorsPerCluster) * blocksPerSector;

  if( deviceWrite( fatDev->device.ownerTID, MINOR(fatDev->deviceNum), blockStart, blocksPerSector * 
                  sectorsPerCluster, FAT_SECTOR_SIZE * sectorsPerCluster, buffer ) == -1 )
  {
    return -1;
  }
  return 0;
}

/* Buffer should be capable of holding 512 bytes (an entire sector). */

int writeSector( struct FAT_Dev *fatDev, unsigned sector, void *buffer )
{
  unsigned blocksPerSector;
  unsigned blockStart;

  if( fatDev == NULL || buffer == NULL )
    return -1;

  blocksPerSector = FAT_SECTOR_SIZE / fatDev->device.dataBlkLen; // FIXME

  blockStart = sector * blocksPerSector;

  if( deviceWrite( fatDev->device.ownerTID, MINOR(fatDev->deviceNum), blockStart, blocksPerSector,
                  FAT_SECTOR_SIZE, buffer ) == -1 )
  {
    return -1;
  }
  return 0;
}

static int getDeviceData( unsigned short devNum, struct FAT_Dev *fatDev )
{
  byte *buffer;

  if( fatDev == NULL )
    return -1;

  if( lookupDevMajor( MAJOR(devNum), &fatDev->device ) != 0 )
  {
    free(buffer);
    return -1;
  }

  if( (buffer = malloc( fatDev->device.dataBlkLen )) == NULL )
    return -1;

  /* Read the first sector and extract the boot parameter block. */

  if( deviceRead( fatDev->device.ownerTID, MINOR(devNum), 0, 1, fatDev->device.dataBlkLen, buffer ) < 0 )
  {
    free(buffer);
    return -1;
  }

  memcpy( &fatDev->bpb, buffer, FAT_SECTOR_SIZE );

  // Using an unconventional sector size

  if( fatDev->bpb.fat12.bytes_per_sec != FAT_SECTOR_SIZE )
  {
/*
    print("Unconventional sector size! (Not 512 bytes)\n");
    print(toIntString(fatDev->bpb.fat12.bytes_per_sec));
    print("\n");*/
    free(buffer);
    return -1;
  }

  // Not a fat device

  if( strncmp( "FAT", (char *)fatDev->bpb.fat12.fs_type, 3 ) != 0 )
  {
    free(buffer);
    return -1;
  }

  /* Fill up the FAT data cache(for speeding up calculations) */

  fatDev->cache.fatType = determineFAT_Type(&fatDev->bpb);

  if( fatDev->cache.fatType == FAT12 )
    fatDev->cache.fatSize = fatDev->bpb.fat12.secs_per_fat;
  else if( fatDev->cache.fatType == FAT16 )
    fatDev->cache.fatSize = fatDev->bpb.fat16.secs_per_fat;
  else
    fatDev->cache.fatSize = fatDev->bpb.fat32.secs_per_fat;

  fatDev->cache.clusterCount = calcClusCount(&fatDev->bpb);
  fatDev->cache.firstDataSec = firstDataSec(&fatDev->bpb);
  fatDev->cache.numDataSecs = calcDataSecs(&fatDev->bpb);
  fatDev->cache.rootDirSecs = calcRootDirSecs(&fatDev->bpb);
  fatDev->deviceNum = devNum;

  free( buffer );

  return 0;
}

/* Follow a path (a set of directory names) until either a file or directory
   is reached. Return the file/directory's directory entry. */

/* Starting with the root directory entry, */

static int getFAT_DirEntry( const char *path, struct FAT_Dev *fatDev, 
                            struct FAT_DirEntry *dirEntry )
{
  unsigned int i;
  int startSector, clusterSize, rootEnts, ret=-1;
  char *buffer, fileName[11], *retFname;
  const char *nextToken;
  struct FAT_DirEntry entry;
  size_t fnameLen, dirSize;
  bool inRootDir=true;
  unsigned cluster=0;

  if( path == NULL || fatDev == NULL || dirEntry == NULL )
    return -1;
  else if( path[0] != '/' )
  {
    // The pathname must start with a slash

    return -1;
  }

  dirSize = FAT_SECTOR_SIZE;

  if(fatDev->cache.fatType == FAT12)
  {
    clusterSize = FAT_SECTOR_SIZE * fatDev->bpb.fat12.secs_per_clus;
    rootEnts = fatDev->bpb.fat12.root_ents;
    startSector = fatDev->bpb.fat12.resd_secs + fatDev->bpb.fat12.secs_per_fat * fatDev->bpb.fat12.fat_copies;
  }
  else if(fatDev->cache.fatType == FAT16)
  {
    clusterSize = FAT_SECTOR_SIZE * fatDev->bpb.fat16.secs_per_clus;
    rootEnts = fatDev->bpb.fat16.root_ents;
    startSector = fatDev->bpb.fat16.resd_secs + fatDev->bpb.fat16.secs_per_fat * fatDev->bpb.fat16.fat_copies;
  }

  nextToken = &path[1];

  fnameLen = get_token_len(nextToken);
  retFname = generate8_3Name(nextToken, fnameLen, fileName );
  nextToken += fnameLen + 1;

  if( retFname == NULL )
  {
    // This is not supposed to happen. The path should lead to a file

    return -1;
  }

  buffer = malloc( clusterSize );	// Assumes that a cluster size >= sector size

  if( buffer == NULL )
    return -1;

  while( true ) /* Go through each root directory entry */
  {
    /* First, read the contents of a directory */

    if( inRootDir )
    {
      if( rootEnts == 0 )
        break;

      if( readSector( fatDev, startSector++, buffer ) != 0 )
        break;
    }
    else
    {
      if( getClusterType(cluster, fatDev->cache.fatType) != USED_CLUSTER )
        break;

      if( readCluster( fatDev, cluster, buffer ) != 0 ) /* Read in a cluster of directory entries */
        break;
    }

    for( i=0; i < dirSize / sizeof entry; i++ )
    {
      if( inRootDir )
      {
        if( rootEnts == 0 )
          break;
        else
          rootEnts--;
      }

      entry = ((struct FAT_DirEntry *)buffer)[i];

      if(entry.filename[0] == (byte)0) /* Reached the end of the directory. End now */
      {
        rootEnts = 0;
        cluster = fatDev->cache.fatType == FAT12 ? 0xFF8 : (fatDev->cache.fatType == FAT16 ? 0xFFF8 : 0xFFFFFF8);
        break;
      }
      else if(entry.filename[0] == (byte)0xE5 || (entry.start_clus == 0 && entry.file_size != 0)) /* Skip deleted file entries */
        continue;
      else if( entry.filename[0] == 0x05 ) // 0x05 corresponds to 0xe5 for the first character
        entry.filename[0] = 0xe5;

      if( strncmp( (char *)entry.filename, fileName, 11 ) == 0 ) /* Found the next directory entry */
      {
        cluster = entry.start_clus;

	fnameLen = get_token_len( nextToken );
	retFname = generate8_3Name( nextToken, fnameLen, fileName );
        nextToken += fnameLen + 1;

        if( retFname == NULL || !(dirEntry->attrib & FAT_SUBDIR) ) // Reached the end of the path or the next item is not a valid directory
        {
          ret = 0;
          *dirEntry = entry;
          goto end_search;
        }
        else
        {
          if( cluster == 0 ) // if the next entry is the root directory...
          {
            inRootDir = true;
            dirSize = FAT_SECTOR_SIZE;

            if(fatDev->cache.fatType == FAT12)
            {
              rootEnts = fatDev->bpb.fat12.root_ents;
              startSector = fatDev->bpb.fat12.resd_secs + fatDev->bpb.fat12.secs_per_fat * fatDev->bpb.fat12.secs_per_fat;
            }
            else if(fatDev->cache.fatType == FAT16)
            {
              rootEnts = fatDev->bpb.fat16.root_ents;
              startSector = fatDev->bpb.fat16.resd_secs + fatDev->bpb.fat16.secs_per_fat * fatDev->bpb.fat16.secs_per_fat;
            }

            break;
          }

          inRootDir = false;
          dirSize = clusterSize;
          break;
        }
      }
    }

    // FIXME: the next cluster should be checked to see if it is used (as it should be)

    if( !inRootDir && i == dirSize / sizeof entry ) // if the last dir entry in the cluster was reached, load the next cluster
      cluster = readFAT( fatDev, cluster );
  }

end_search:

  free( buffer );
  return ret;
}

int fatReadFile( const char *path, unsigned int offset, unsigned short devNum,
                 char *fileBuffer, size_t length )
{
  int written=0;
  char *buffer;
  unsigned cluster, clusterSize, clusterOffset;
  struct FAT_DirEntry entry;
  struct FAT_Dev fatInfo, *fat_dev = &fatInfo;

  if( path == NULL || fat_dev == NULL || getDeviceData( devNum, &fatInfo ) < 0 )
    return -1;

  if( getFAT_DirEntry( path, fat_dev, &entry ) != 0 )
    return -1;

  if( offset >= entry.file_size )
    return 0;

  if( entry.attrib & FAT_SUBDIR )
    return -1;

  cluster = entry.start_clus;
  clusterSize = calcClusterSize( &fat_dev->bpb );
  clusterOffset = offset % clusterSize;

  for( unsigned i=offset / clusterSize; i > 0; i-- )
    cluster = readFAT( fat_dev, cluster ); // FIXME: may cause errors

  buffer = malloc( clusterSize );

  if( buffer == NULL )
    return -1;

  while( getClusterType( cluster, fat_dev->cache.fatType ) == USED_CLUSTER && 
         length > 0 && entry.file_size > 0 )
  {
    readCluster( fat_dev, cluster, buffer );

    if( length > (clusterSize - clusterOffset) && entry.file_size > clusterSize ) /* If file buffer and file entry is greater than the size of a whole cluster..., copy a whole cluster */
    {
      memcpy( fileBuffer, buffer + clusterOffset, clusterSize - clusterOffset );
      fileBuffer += (clusterSize - clusterOffset);
      length -= (clusterSize - clusterOffset);
      entry.file_size -= clusterSize;
      written += (clusterSize - clusterOffset);
    }
    else if( (length > (clusterSize - clusterOffset) && entry.file_size <= clusterSize) || 
             (length <= (clusterSize - clusterOffset) && entry.file_size <= clusterSize &&
               entry.file_size <= length) ) // If there's enough buffer space, but the remaining file data is less than the size of a cluster AND the remaining file data can fit in the remaining buffer space
    {
      memcpy( fileBuffer, buffer + clusterOffset, entry.file_size - clusterOffset );
      written += entry.file_size - clusterOffset;
      entry.file_size = 0;
      break;
    }
    else
    {
      memcpy( fileBuffer, buffer + clusterOffset, length );
      written += length;
      length = 0;
      break;
    }

    clusterOffset = 0;
    cluster = readFAT( fat_dev, cluster );
  }

  free( buffer );
  return written;
}

int fatGetDirList( const char *path, unsigned short devNum, struct FileAttributes *attrib, 
                   size_t maxEntries )
{
//  struct FAT_DirEntry *entry, directory;
//  int i;
//  enum FAT_Type type;
//  unsigned clusterSize, dirSize, cluster, sector, numRootEnts;
//  char *buffer;
//  bool isRootDir = false, done=false;
  unsigned nameLen, extLen;
  struct FileAttributes *fsEntry = attrib;
  struct FAT_Dev fatInfo, *fat_dev = &fatInfo;
  int entriesListed=0, entriesRead;
  struct FatDir *fat_dir;

  if( path == NULL || attrib == NULL || getDeviceData( devNum, &fatInfo ) < 0 )
    return -1;

  fat_dir = readDirEntry( path, fat_dev );

  if( fat_dir == NULL )
    return -1;

  while( maxEntries )
  {
    entriesRead = 0;

    for( unsigned i=0; i < fat_dir->num_entries && i < maxEntries; i++, fsEntry++ )
    {
      if(fat_dir->entries[i].filename[0] == (byte)0xE5 || 
        (fat_dir->entries[i].start_clus == 0 && fat_dir->entries[i].file_size != 0)) // Disregard deleted file entries and LFNs
      {
        fsEntry--;
        continue;
      }
      else if( fat_dir->entries[i].filename[0] == 0x05 )
        fat_dir->entries[i].filename[0] = 0xe5;

      memset( fsEntry, 0, sizeof *fsEntry );
      fsEntry->size = fat_dir->entries[i].file_size;

      for( nameLen=8; nameLen >= 1 && fat_dir->entries[i].filename[nameLen-1] == ' '; nameLen-- );
      strncpy( fsEntry->name, (char *)fat_dir->entries[i].filename, nameLen );

      fsEntry->timestamp = 0; // FIXME
      fsEntry->nameLen += nameLen;

      if( fat_dir->entries[i].attrib & FAT_RDONLY )
        fsEntry->flags |= FS_RDONLY;
      if( fat_dir->entries[i].attrib & FAT_HIDDEN )
        fsEntry->flags |= FS_HIDDEN;

      if(fat_dir->entries[i].attrib & FAT_SUBDIR)
        fsEntry->flags |= FS_DIR;
      else
      {
        for( extLen = 3; extLen >= 1 && fat_dir->entries[i].filename[7 + extLen] == ' '; extLen-- );

        if( extLen )
        {
          fsEntry->name[nameLen] = '.';
          strncpy( &fsEntry->name[nameLen+1], (char *)&fat_dir->entries[i].filename[8], extLen );
          fsEntry->nameLen += extLen + 1;
        }
      }

      entriesRead++;
    }

    if( maxEntries > entriesRead )
    {
      maxEntries -= entriesRead;
      entriesListed += entriesRead;
    }
    else
    {
      entriesListed += maxEntries;
      maxEntries = 0;
    }

    if( fat_dir->more )
    {
      fat_dir = readNextEntry( fat_dir );

      if( fat_dir == NULL )
      {
        freeFatDir( fat_dir );
        return entriesListed;
      }
    }
    else
    {
      freeFatDir( fat_dir );
      return entriesListed;
    }
  }

  freeFatDir( fat_dir );
  return -1;
}

int fatGetAttributes( const char *path, unsigned short devNum, struct FileAttributes *attrib )
{
  struct FAT_DirEntry entry;
  size_t nameLen, extLen;
  struct FAT_Dev fatInfo, *fat_dev = &fatInfo;

  if( path == NULL || attrib == NULL || getDeviceData( devNum, &fatInfo ) < 0 )
    return -1;

  if( getFAT_DirEntry( path, fat_dev, &entry ) != 0 )
    return -1;

  memset( attrib, 0, sizeof *attrib );
  attrib->size = entry.file_size;

  for( nameLen=8; nameLen >= 1 && entry.filename[nameLen-1] == ' '; nameLen-- );
  strncpy( attrib->name, (char *)entry.filename, nameLen );

  attrib->timestamp = 0;
  attrib->nameLen += nameLen;

  if( entry.attrib & FAT_RDONLY )
    attrib->flags |= FS_RDONLY;
  if( entry.attrib & FAT_HIDDEN )
    attrib->flags |= FS_HIDDEN;

  if(entry.attrib & FAT_SUBDIR)
    attrib->flags |= FS_DIR;
  else
  {
    for( extLen = 3; extLen >= 1 && entry.filename[7 + extLen] == ' '; extLen-- );

    if( extLen )
    {
      attrib->name[nameLen] = '.';
      strncpy( &attrib->name[nameLen+1], (char *)&entry.filename[8], extLen );
      attrib->nameLen += extLen + 1;
    }
  }
  return 0;
}

int fatCreateFile( const char *path, const char *name, unsigned short devNum )
{
  struct FAT_Dev fatInfo;

  if( path == NULL || name == NULL || getDeviceData( devNum, &fatInfo ) < 0 )
    return -1;

  return _createEntry( path, name, &fatInfo, false );
}

int fatCreateDir( const char *path, const char *name, unsigned short devNum )
{
  struct FAT_Dev fatInfo;

  if( path == NULL || name == NULL || getDeviceData( devNum, &fatInfo ) < 0 )
    return -1;

  return _createEntry( path, name, &fatInfo, true );
}

/* This needs to be redone */

static int _createEntry( const char *path, const char *name, struct FAT_Dev *fat_dev, bool dir )
{
  struct FAT_DirEntry *entry, directory;
  unsigned int i;
  enum FAT_Type type;
  unsigned dirSize, cluster, sector, numRootEnts;
  char *buffer;
  bool isRootDir = false;
  bool endEntry = false;
  char newFname[11];

  if( generate8_3Name( name, strlen(name), newFname ) == NULL )
    return -1;

  if( strcmp( path, "/" ) == 0 )
  {
    isRootDir = true;
    type = determineFAT_Type( &fat_dev->bpb );

    dirSize = FAT_SECTOR_SIZE;

    if( type == FAT12 )
    {
      numRootEnts = fat_dev->bpb.fat12.root_ents;
      sector = fat_dev->bpb.fat12.resd_secs + fat_dev->bpb.fat12.secs_per_fat * 
               fat_dev->bpb.fat12.fat_copies;
    }
    else if( type == FAT16 )
    {
      numRootEnts = fat_dev->bpb.fat16.root_ents;
      sector = fat_dev->bpb.fat16.resd_secs + fat_dev->bpb.fat16.secs_per_fat * 
               fat_dev->bpb.fat16.fat_copies;
    }

    buffer = malloc( dirSize );

    if( buffer == NULL )
      return -1;
  }
  else
  {
    if( getFAT_DirEntry( path, fat_dev, &directory ) || 
        !(directory.attrib & FAT_SUBDIR) )
    {
      return -1;
    }

    dirSize = calcClusterSize( &fat_dev->bpb );

    buffer = malloc( dirSize );

    if( buffer == NULL )
      return -1;

    cluster = directory.start_clus;
  }

  entry = (struct FAT_DirEntry *)buffer;

  while( true )
  {
    if( isRootDir )
    {
      if( numRootEnts == 0 )
        goto fat_error;

      if( readSector( fat_dev, sector++, buffer ) != 0 )
        goto fat_error;
    }
    else
    {
      enum FAT_ClusterType clusterType = getClusterType(cluster, fat_dev->cache.fatType);

      if( clusterType == END_CLUSTER )
        goto fat_error;
      else if( clusterType != USED_CLUSTER )
        goto fat_error;

      if( readCluster( fat_dev, cluster, buffer ) != 0 )
        goto fat_error;
    }

    for(i=0; i < dirSize / sizeof(struct FAT_DirEntry); i++, entry++)
    {
      if( isRootDir && numRootEnts-- == 0 )
        goto fat_error;

      /* Go through each entry and try to find the entry after the last or
         a deleted entry */

      if(entry->filename[0] == (byte)0 || entry->filename[0] == (byte)0xE5)
      {
        if( entry->filename[0] == (byte)0 )
          endEntry = true;

        strncpy((char *)entry->filename, newFname, 11);

        entry->attrib = dir ? FAT_SUBDIR : 0;
        entry->time = entry->date = 0;
        entry->start_clus = fat_dev->cache.fatType == FAT12 ? 0xFF8 : 
                      (fat_dev->cache.fatType == FAT16 ? 0xFFF8 : 0xFFFFFF8);
        entry->file_size = dir ? dirSize : 0;

        if( dir )
        {
          unsigned dirCluster = getFreeCluster( fat_dev );
          struct FAT_DirEntry *entry2;

          if( dirCluster == 1 || writeFAT( fat_dev, fat_dev->cache.fatType == FAT12 ? 0xFF8 : 
                      (fat_dev->cache.fatType == FAT16 ? 0xFFF8 : 0xFFFFFF8), dirCluster ) < 0 )
          {
            // Unable to get a free cluster for the directory or writeFAT() failed

            goto fat_error;
          }

          byte *buffer2 = malloc(calcClusterSize(&fat_dev->bpb));

          if( buffer2 == NULL )
          {
            // FIXME: The previous writeFAT() should be reversed, otherwise the FAT will be corrupted.
            goto fat_error;
          }

          if( readCluster( fat_dev, dirCluster, buffer2 ) < 0 )
          {
            free(buffer2);

            // FIXME: The previous writeFAT() should be reversed, otherwise the FAT will be corrputed.
            goto fat_error;
          }

	  entry2 = (struct FAT_DirEntry *)buffer2;

	  memset(entry2, 0, 3*sizeof(struct FAT_DirEntry));

          memset(entry2[0].filename, ' ', 11);
          memset(entry2[1].filename, ' ', 11);

          entry2[0].filename[0] = '.';
          entry2[1].filename[0] = entry2[1].filename[1] = '.';
          entry2[2].filename[0] = (byte)0;

          entry2[0].time = entry2[0].date = entry2[1].time = entry2[1].date = 0;
          entry2[0].attrib = entry2[1].attrib = FAT_SUBDIR;
          entry2[0].start_clus = dirCluster;
          entry2[0].file_size = fat_dev->cache.rootDirSecs * (FAT_SECTOR_SIZE / sizeof(struct FAT_DirEntry));
          entry2[1].start_clus = 0;
          entry2[1].file_size = calcClusterSize( &fat_dev->bpb );

          if( writeCluster( fat_dev, dirCluster, buffer2 ) != 0 )
          {
            // FIXME: The previous writeFAT() should be reversed, otherwise the FAT will be corrputed.
            goto fat_error;
          }

          free( buffer2 );
          entry->start_clus = dirCluster;
        }

        if( endEntry )
        {
          /* After finding an end entry, it must be replaced with a new one.
             The new end entry might be on another cluster however... */

          if( i + 1 == dirSize / sizeof(struct FAT_DirEntry) ) // If it's the last entry in the cluster/sector
          {
            if( isRootDir ) // we're in the root directory. only sectors are used here
            {
              if( numRootEnts == 0 || writeSector( fat_dev, sector - 1, buffer ) < 0 )
                goto fat_error;

              if( readSector( fat_dev, sector, buffer ) != 0 )
                goto fat_error;

              entry = (struct FAT_DirEntry *)buffer;

              entry->filename[0] = (byte)0;

              if( writeSector( fat_dev, sector, buffer ) != 0 )
              {
                /* FIXME: If the end directory entry is unable to be written onto the new sector,
                   then the previous operations need to be undone, otherwise the FAT will be corrputed. */
                goto fat_error;
              }
            }
            else 
            {
              unsigned cluster2 = readFAT( fat_dev, cluster );

              enum FAT_ClusterType clusType = getClusterType( cluster2, fat_dev->cache.fatType );

              if( clusType == END_CLUSTER ) // This should never occur
              {
                if( (cluster2 = getFreeCluster( fat_dev )) == 1 || 
                  writeFAT( fat_dev, cluster2, cluster) == -1 )
                {
                  goto fat_error;
                }

                if( writeFAT( fat_dev, fat_dev->cache.fatType == FAT12 ? 0xFF8 : 
                      (fat_dev->cache.fatType == FAT16 ? 0xFFF8 : 0xFFFFFF8), cluster2 ) < 0 )
                {
               // FIXME: The previous operation should be reversed, otherwise the FAT will be corrupted.
                  goto fat_error;
                }
              }

              if( writeCluster( fat_dev, cluster, buffer ) < 0 )
              {
              // FIXME: The previous writeFAT() should be reversed, otherwise the FAT will be corrupted.
                goto fat_error;
              }

              if( readCluster( fat_dev, cluster2, buffer ) != 0 )
              {
              // FIXME: The previous writeCluster() should be reversed, otherwise the FAT will be corrupted.
                goto fat_error;
              }

              entry = (struct FAT_DirEntry *)buffer;

              entry->filename[0] = (byte)0;

              if( writeCluster( fat_dev, cluster2, buffer ) != 0 )
              {
            // FIXME: The previous writeCluster() should be reversed, otherwise the FAT will be corrupted.
                goto fat_error;
              }
            }
          }
          else // Simply update the next entry and write the cluster back to disk.
          {
            entry++;
            entry->filename[0] = (byte)0;

             if( isRootDir )
             {
               if( writeSector( fat_dev, sector - 1, buffer ) != 0 )
               {
               // FIXME: The previous operation should be reversed, otherwise the FAT will be corrupted.
                 goto fat_error;
               }
             }
             else
             {
               if( writeCluster( fat_dev, cluster, buffer ) < 0 )
               {
               // FIXME: The previous operation should be reversed, otherwise the FAT will be corrupted.
                 goto fat_error;
               }
             }
          }
        }

        free(buffer);
        return 0;
      }
      else
        continue;
    }

    if( !isRootDir )
      cluster = readFAT( fat_dev, cluster );
  }

fat_error:
  free(buffer);
  return -1;
}

static struct FatDir *readDirEntry( const char *path, struct FAT_Dev *fat_dev )
{
  enum FAT_Type type;
  unsigned max_entries;
  struct FatDir *fat_dir;
  struct FAT_DirEntry directory;
  unsigned numRootEnts;

  fat_dir = malloc( sizeof( struct FatDir ) );

  if( fat_dir == NULL )
    return NULL;

  fat_dir->fatDev = fat_dev;

  if( strcmp( path, "/" ) == 0 ) // If we're reading from the root dir...
  {
inRootDirectory:

    type = determineFAT_Type( &fat_dev->bpb );

    if( type == FAT12 )
    {
      numRootEnts = fat_dev->bpb.fat12.root_ents;
      fat_dir->sector = fat_dev->bpb.fat12.resd_secs + fat_dev->bpb.fat12.secs_per_fat * 
               fat_dev->bpb.fat12.fat_copies;
    }
    else if( type == FAT16 )
    {
      numRootEnts = fat_dev->bpb.fat16.root_ents;
      fat_dir->sector = fat_dev->bpb.fat16.resd_secs + fat_dev->bpb.fat16.secs_per_fat * 
               fat_dev->bpb.fat16.fat_copies;
    }

    fat_dir->use_sects = 1;
    max_entries = FAT_SECTOR_SIZE / sizeof( struct FAT_DirEntry );
    fat_dir->entries = malloc( FAT_SECTOR_SIZE );

    if( numRootEnts >= max_entries )
    {
      fat_dir->next = fat_dir->sector + 1;
      fat_dir->more = 1;
    }
    else
      fat_dir->more = 0;

    if( fat_dir->entries == NULL || 
        readSector( fat_dev, fat_dir->sector, fat_dir->entries ) < 0 )
    {
      goto fat_err;
    }
  }
  else
  {
    if( getFAT_DirEntry( path, fat_dev, &directory ) != 0 || 
        !(directory.attrib & FAT_SUBDIR) )
    {
      goto fat_err;
    }

    if( directory.start_clus == 0 && directory.file_size == 0 )
      goto inRootDirectory; // I know I shouldn't, but...
    else
    {
      if( getClusterType(directory.start_clus, fat_dev->cache.fatType) != USED_CLUSTER )
        goto fat_err;

      fat_dir->use_sects = 0;
      max_entries = calcClusterSize(&fat_dev->bpb) / sizeof( struct FAT_DirEntry );
      fat_dir->entries = malloc( max_entries * sizeof( struct FAT_DirEntry ) );
      fat_dir->cluster = directory.start_clus;
      fat_dir->next = readFAT( fat_dev, fat_dir->cluster );

      if( fat_dir->entries == NULL )
        goto fat_err;
    }
  }

  for( unsigned i=0; i < max_entries; i++ )
  {
    if( fat_dir->entries[i].filename[0] == 0 )
    {
      fat_dir->num_entries = i;
      fat_dir->more = 0;
      break;
    }

    if( i == max_entries - 1 )
    {
      fat_dir->num_entries = max_entries;
      fat_dir->more = 1;
    }
  }
 
  return fat_dir;

fat_err:
  free(fat_dir);
  return NULL;
}

static struct FatDir *readNextEntry( struct FatDir *fat_dir )
{
  unsigned max_entries, numRootEnts, startSec;
  enum FAT_Type type;

  if( !fat_dir->more )
    return NULL;

  if( fat_dir->use_sects )
  {
inRootDirectory:
    fat_dir->sector++;
    fat_dir->next++;

    type = determineFAT_Type( &fat_dir->fatDev->bpb );

    if( type == FAT12 )
    {
      numRootEnts = fat_dir->fatDev->bpb.fat12.root_ents;
      startSec = fat_dir->fatDev->bpb.fat12.resd_secs + fat_dir->fatDev->bpb.fat12.secs_per_fat * 
               fat_dir->fatDev->bpb.fat12.fat_copies;
    }
    else if( type == FAT16 )
    {
      numRootEnts = fat_dir->fatDev->bpb.fat16.root_ents;
      startSec = fat_dir->fatDev->bpb.fat16.resd_secs + fat_dir->fatDev->bpb.fat16.secs_per_fat * 
               fat_dir->fatDev->bpb.fat16.fat_copies;
    }

    max_entries = FAT_SECTOR_SIZE / sizeof( struct FAT_DirEntry );

    if( numRootEnts >= max_entries + (fat_dir->sector - startSec) * 
        sizeof(struct FAT_DirEntry) )
    {
      fat_dir->more = 1;
    }
    else
      fat_dir->more = 0;

    if( readSector( fat_dir->fatDev, fat_dir->sector, fat_dir->entries ) < 0 )
    {
      goto fat_err;
    }
  }
  else
  {
    if( fat_dir->cluster == 0 )
      goto inRootDirectory; // I know I shouldn't, but...
    else
    {
      if( getClusterType(fat_dir->cluster, fat_dir->fatDev->cache.fatType) != 
          USED_CLUSTER )
      {
        goto fat_err;
      }

      max_entries = calcClusterSize(&fat_dir->fatDev->bpb) 
                    / sizeof( struct FAT_DirEntry );
      fat_dir->next = readFAT( fat_dir->fatDev, fat_dir->cluster );
    }
  }

  for( unsigned i=0; i < max_entries; i++ )
  {
    if( fat_dir->entries[i].filename[0] == 0 )
    {
      fat_dir->num_entries = i;
      fat_dir->more = 0;
      break;
    }

    if( i == max_entries - 1 )
    {
      fat_dir->num_entries = max_entries;

      if( getClusterType(fat_dir->cluster, fat_dir->fatDev->cache.fatType) !=
          USED_CLUSTER )
      {
        fat_dir->more = 0;
      }
      else
        fat_dir->more = 1;
    }
  }

  return fat_dir;

fat_err:
  free(fat_dir);
  return NULL;
}

static void freeFatDir( struct FatDir *fat_dir )
{
  free(fat_dir->entries);
  free(fat_dir);
}