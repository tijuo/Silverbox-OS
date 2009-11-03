#include <fatfs.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <vfs.h>
#include <dev_interface.h>
#include <os/services.h>

/* XXX: Need to implement LFNs and FAT32 */

extern int writeFAT(struct FAT_Dev *fatDev, dword entry, unsigned cluster);
extern unsigned readFAT(struct FAT_Dev *fatDev, unsigned cluster);
extern int getFreeCluster(struct FAT_Dev *fatDev);

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
static int getFAT_DirEntry( const char *path, struct FAT_Dev *fatDev, struct FAT_DirEntry *dirEntry );

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

  if( nameLen == 1 && name[0] == '.' )
  {
    if( name[0] == '.' )
    {
      filename[0] = '.';
      memset( &filename[1], ' ', 10 );
    }
    else
      return NULL;
  }
  else if( nameLen == 2 )
  {
    if( name[0] == '.' && name[1] == '.' )
    {
      filename[0] = filename[1] = '.';
      memset( &filename[2], ' ', 9 );
    }
    else
      return NULL;
  }
  else
  {
    unsigned dotIndex;

    for(dotIndex=0; dotIndex < nameLen; dotIndex++ )
    {
      if( name[dotIndex] == '.' )
        break;
      else if( !checkFAT_Char( name[dotIndex] ) )
        return NULL;
    }

    if( dotIndex == nameLen ) // If we couldn't find the dot
    {
      if( nameLen > 8 )
        return NULL;

      memcpy( filename, name, nameLen );
      memset( &filename[nameLen], ' ', 11-nameLen );
    }
    else // if we found the dot
    {
      size_t extLen;

      if( dotIndex > 8 )
        return NULL;

      memcpy( filename, name, dotIndex );
      memset( &filename[dotIndex], ' ', 8 - dotIndex);

      extLen = nameLen-dotIndex-1;

      memcpy( &filename[8], &name[dotIndex+1], extLen );
      memset( &filename[8+extLen], ' ', 3-extLen );
    }
  }

  for(i=0; i < 11; i++)
    new_name[i] = (islower(filename[i]) ? toupper(filename[i]) : filename[i]);

  return new_name;
}

/* This may modify the path string. */

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
    return 1; // unsupported

  blocksPerSector = FAT_SECTOR_SIZE / fatDev->device.dataBlkLen; // Flawed

  blockStart = (fatDev->cache.firstDataSec + (cluster-2) * sectorsPerCluster) * blocksPerSector;

  if( deviceRead( fatDev->device.ownerTID, MINOR(fatDev->deviceNum), blockStart, blocksPerSector * sectorsPerCluster,
                  FAT_SECTOR_SIZE * sectorsPerCluster, buffer ) == -1 )
  {
    printMsg("deviceRead() fail\n");
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

  blocksPerSector = FAT_SECTOR_SIZE / fatDev->device.dataBlkLen; // Flawed

  blockStart = sector * blocksPerSector;

  if( deviceRead( fatDev->device.ownerTID, MINOR(fatDev->deviceNum), blockStart, blocksPerSector,
                  FAT_SECTOR_SIZE, buffer ) == -1 )
  {
    printMsg("deviceRead() fail\n");
    return -1;
  }
  return 0;
}

static int getDeviceData( unsigned short devNum, struct FAT_Dev *fatDev )
{
  if( fatDev == NULL )
    return -1;

  if( lookupDevMajor( MAJOR(devNum), &fatDev->device ) != 0 )
    return -1;

  /* Read the first sector and extract the boot parameter block. */

  if( deviceRead( fatDev->device.ownerTID, MINOR(devNum), 0, 1, 512, &fatDev->bpb ) < 0 )
    return -1;

  if( fatDev->bpb.fat12.bytes_per_sec != FAT_SECTOR_SIZE )
  {
    print("Unconventional sector size! (Not 512 bytes)\n");
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

  return 0;
}

// XXX: FAT32/LFN not supported

static int getFAT_DirEntry( const char *path, struct FAT_Dev *fatDev, 
                            struct FAT_DirEntry *dirEntry )
{
  int i;
  int startSector, clusterSize, rootEnts, ret=-1;
  char *buffer, *nextToken, fileName[11], *retFname;
  struct FAT_DirEntry entry;
  size_t fnameLen, dirSize;
  bool inRootDir=true;
  unsigned cluster=0;

  if( path == NULL || fatDev == NULL || dirEntry == NULL )
  {
    printMsg("Null path, fat_dev, or dirEntry\n");
    return -1;
  }

  else if( path[0] != '/' )
  {
    printMsg("Code requires the pathname to start with a '/'");
    return -1;
  }

  dirSize = FAT_SECTOR_SIZE;

  if(fatDev->cache.fatType == FAT12)
  {
    clusterSize = dirSize * fatDev->bpb.fat12.secs_per_clus;
    rootEnts = fatDev->bpb.fat12.root_ents;
    startSector = fatDev->bpb.fat12.resd_secs + fatDev->bpb.fat12.secs_per_fat * fatDev->bpb.fat12.fat_copies;
  }
  else if(fatDev->cache.fatType == FAT16)
  {
    clusterSize = dirSize * fatDev->bpb.fat16.secs_per_clus;
    rootEnts = fatDev->bpb.fat16.root_ents;
    startSector = fatDev->bpb.fat16.resd_secs + fatDev->bpb.fat16.secs_per_fat * fatDev->bpb.fat16.fat_copies;
  }

  nextToken = &path[1];

  fnameLen = get_token_len(nextToken);
  retFname = generate8_3Name(nextToken, fnameLen, fileName );
  nextToken += fnameLen + 1;

  if( retFname == NULL )
  {
    printMsg("This is not supposed to happen: ");
    printMsg("Path doesn't lead to a file\n");

    return -1;
  }

  buffer = malloc( clusterSize );

  if( buffer == NULL )
    return -1;

  while( true ) /* Go through each root directory entry */
  {
    if( inRootDir )
    {
      if( rootEnts == 0 )
        break;

      if( readSector( fatDev, startSector++, buffer ) != 0 )
      {
        printMsg("readSector() failed.\n");
        break;
      }
    }
    else
    {
      if( getClusterType(cluster, fatDev->cache.fatType) != USED_CLUSTER )
        break;

      if( readCluster( fatDev, cluster, buffer ) != 0 ) /* Read in a cluster of directory entries */
      {
        printMsg("readCluster() failed.\n");
        break;
      }
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
//        printMsg("Reached end of directory\n");
        rootEnts = 0;
        cluster = fatDev->cache.fatType == FAT12 ? 0xFF8 : (fatDev->cache.fatType == FAT16 ? 0xFFF8 : 0xFFFFFF8);
        break;
      }
      else if(entry.filename[0] == (byte)0xE5 || entry.start_clus < 2 ) /* Disregard deleted file entries */
        continue;
      else if( entry.filename[0] == 0x05 )
        entry.filename[0] = 0xe5;

      if( strncmp( (char *)entry.filename, fileName, 11 ) == 0 ) /* Found the directory entry */
      {
        cluster = entry.start_clus;

	fnameLen = get_token_len( nextToken );
	retFname = generate8_3Name( nextToken, fnameLen, fileName );
        nextToken += fnameLen + 1;

        if( retFname == NULL || !(dirEntry->attrib & FAT_SUBDIR) )
        {
          ret = 0;
          *dirEntry = entry;
          goto end_search;
        }
        else
        {
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
    cluster = readFAT( fat_dev, cluster );

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
  struct FAT_DirEntry *entry, directory;
  int i;
  enum FAT_Type type;
  unsigned clusterSize, dirSize, cluster, sector, numRootEnts;
  char *buffer, *pathName;
  bool isRootDir = false, done=false;
  int numEntries = 0, nameLen, extLen;
  struct FileAttributes *fsEntry = attrib;
  struct FAT_Dev fatInfo, *fat_dev = &fatInfo;

  if( path == NULL || attrib == NULL || getDeviceData( devNum, &fatInfo ) < 0 )
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

    dirSize = clusterSize = calcClusterSize( &fat_dev->bpb );

    buffer = malloc( dirSize );

    if( buffer == NULL )
      return -1;

    cluster = directory.start_clus;
  }

  entry = (struct FAT_DirEntry *)buffer;

  while( !done )
  {
    if( isRootDir )
    {
      if( numRootEnts == 0 )
      {
        done = true;
        break;
      }

      if( readSector( fat_dev, sector++, buffer ) != 0 )
      {
        printMsg("readSector() failed.\n");
        free( buffer );
        return -1;
      }
    }
    else
    {
      enum FAT_ClusterType clusterType = getClusterType(cluster, fat_dev->cache.fatType);

      if( clusterType == END_CLUSTER )
      {
        done = true;
        break;
      }
      else if( clusterType != USED_CLUSTER )
      {
        free( buffer );
        return -1;
      }

      if( readCluster( fat_dev, cluster, buffer ) != 0 )
      {
        printMsg("readCluster() failed.\n");
        free( buffer );
        return -1;
      }
    }

    for(i=0; i < dirSize / sizeof(struct FAT_DirEntry); i++, entry++)
    {
      if( isRootDir && numRootEnts-- == 0 )
      {
        done = true;
        break;
      }
      //fsEntry = (struct FileAttributes *)((char *)fsEntry + fsEntry->entryLen);
      if(entry->filename[0] == (byte)0)
      {
        done = true;
        break;
      }
      else if(entry->filename[0] == (byte)0xE5 || entry->start_clus == 0) // Disregard deleted file entries 
        continue;
      else if( entry->filename[0] == 0x05 )
        entry->filename[0] = 0xe5;

      fsEntry->size = entry->file_size;

      for( nameLen=8; nameLen >= 1 && entry->filename[nameLen-1] == ' '; nameLen-- );
      strncpy( fsEntry->name, (char *)entry->filename, nameLen );

      fsEntry->timestamp = 0;
      fsEntry->nameLen += nameLen;

      if( entry->attrib & FAT_RDONLY )
        fsEntry->flags |= FS_RDONLY;
      if( entry->attrib & FAT_HIDDEN )
        fsEntry->flags |= FS_HIDDEN;

      if(entry->attrib & FAT_SUBDIR)
        fsEntry->flags |= FS_DIR;
      else
      {
        for( extLen = 3; extLen >= 1 && entry->filename[7 + extLen] == ' '; extLen-- );

        if( extLen )
        {
          fsEntry->name[nameLen] = '.';
          strncpy( &fsEntry->name[nameLen+1], (char *)&entry->filename[8], extLen );
          fsEntry->nameLen += extLen + 1;
        }
      }

      maxEntries--;

      fsEntry++;
      numEntries++;
    }

    if( !isRootDir )
      cluster = readFAT( fat_dev, cluster );
  }

  free(buffer);

  return numEntries;
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

int main(void)
{
return 0;
}