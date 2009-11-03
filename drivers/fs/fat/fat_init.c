#include <device.h>
#include <dev_interface.h>
#include <fatfs.h>
#include <fs.h>
#include <vfs.h>
#include <os/services.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <os/message.h>

#define FAT_FILENAME_LEN	11

#define MAJOR(x) ((x >> 8) & 0xFF)
#define MINOR(x) (x & 0xFF)

#define SERVER_NAME		"fat"
#define FS_NAME			"fat"

/*
extern int fat_list(struct vdir *parent, struct vfile *flist, struct vdir *dlist);
int fat_init(struct vdir *root, int device, int flags);
//extern int Mount(char *fsname, int dev, char *mntpoint, int flags);
extern void RegisterFS(struct filesystem *fs);

extern struct file_ops fat_fops;
extern struct dir_ops fat_dops;
*/

/* This needs to be able to talk to the device manager */

int fatReadFile( char *path, unsigned short devNum, unsigned int offset, size_t bytes, char *buffer );
int fatGetDirList( char *path, unsigned short devNum, char *buffer, size_t bufferLen );
int fatGetAttributes( char *path, unsigned short devNum, char *buffer, size_t bufferLen, struct FileAttributes *attrib );

int readFile( char *path, unsigned int offset, struct Device *device, struct FAT_Dev *fat_dev, char *fileBuffer, int fBufferLen );
int readSector( struct Device *device, struct FAT_Dev *fat_dev, unsigned sector, void *buffer );
int readCluster( struct Device *device, struct FAT_Dev *fat_dev, unsigned cluster, void *buffer );
int getFAT_DirEntry( char *path, struct Device *device, struct FAT_Dev *fat_dev, struct FAT_DirEntry *entry );

/* For writing to an existing file, you can place data into new clusters and when that's done,
   change the start cluster of the file. */

struct FAT_DevInfo
{
  struct Device device;
  struct FAT_Dev fatInfo;
  struct FAT_DevInfo *next;
};

struct FAT_DevInfo *fatDevList;
struct FAT_DevInfo *lookupDeviceInfo( unsigned short devNum );
struct FAT_DevInfo *addDeviceInfo( unsigned short devNum, int flags );

/*
bool isValidElfExe( addr_t img )
{
        elf_header_t *image = ( elf_header_t * ) img;

        if( img == NULL )
                return false;

        if ( memcmp( "\x7F""ELF", image->identifier, 4 ) != 0 )
                return false;

        else
        {
                if ( image->identifier[ EI_VERSION ] != EV_CURRENT )
                        return false;
                if ( image->type != ET_EXEC )
                        return false;
                if ( image->machine != EM_386 )
                        return false;
                if ( image->version != EV_CURRENT )
                        return false;
                if ( image->identifier[ EI_CLASS ] != ELFCLASS32 )
                        return false;
        }

        return true;

}

int load_elf_exec( char *path, int devNum )
{
        unsigned phtab_count, i, j;
        elf_pheader_t *pheader;
	char *file = (char *)0x600000;
	elf_header_t *image = (elf_header_t *)file;
	pid_t pid;

	allocatePages( file, 256 );

        fatReadFile( path, devNum, 0, 1024 * 1024, file );

        if( !isValidElfExe( file ) )
        {
                printMsg("Not a valid ELF executable.\n");
                return -1;
        }

        phtab_count = image->phnum;

        pid = __createProcess( 1 );

        / * Program header information is loaded into memory * /

        pheader = (elf_pheader_t *)(file + image->phoff);

        for ( i = 0; i < phtab_count; i++, pheader++ )
        {
                if ( pheader->type == PT_LOAD )
                {
                        unsigned memSize = pheader->memsz;
                        unsigned fileSize = pheader->filesz;

                        for( j = 0; memSize > 0; j++ )
                        {
                                if ( fileSize == 0 )
				  memset( (void *)(file + pheader->offset + 0x1000 * j), 0, (memSize > 4096 ? 4096 : memSize) );

				__grantMem( file + pheader->offset + 0x1000 * j, pheader->vaddr + 0x1000 * j, pid, 1 );

                                if( memSize < 4096 )
                                  memSize = 0;
                                else
                                  memSize -= 4096;

                                if( fileSize < 4096 )
                                  fileSize = 0;
                                else
                                  fileSize -= 4096;

                        }
                }
        }

	__createThread( (void *)image->entry, pid );

        return 0;
}
*/

tid_t video_srv = NULL_TID;

int printMsg( char *msg )
{
  if( video_srv == NULL_TID )
    video_srv = lookupName("video", strlen("video"));

  return deviceWrite( video_srv, 0, 0, strlen(msg), 1, msg );
}

int printMsgN( char *msg, int len )
{
  if( video_srv == NULL_TID )
    video_srv = lookupName("video", strlen("video"));

  return deviceWrite( video_srv, 0, 0, len, 1, msg );
}

int fatRegister()
{
  return -1;
}

/* The file data are in the clusters. The FAT will tell what the next
   file cluster is for a cluster.
   Ex.

  Start clus: 40

  FAT[40] = 80
  FAT[80] = 100
  FAT[100] = 0xFFF

  File is in clusters 40, 80, 100.

  A directory is a special file that contains directory entries

 */

char fbuffer[1024];

int fatGetDirList( char *path, unsigned short devNum, char *buffer, size_t bufferLen )
{
  struct FAT_DevInfo *devInfo;

  if( path == NULL || buffer == NULL )
    return -1;

  devInfo = lookupDeviceInfo( devNum );

  if( devInfo == NULL )
  {
    if( (devInfo = addDeviceInfo( devNum, 0 )) == NULL )
      return -1;
  }

  return getDirList( path, &devInfo->device, &devInfo->fatInfo, buffer, bufferLen );
}

/*
int fatReadFile( char *path, unsigned short devNum, unsigned int offset, size_t bytes, char *buffer)
{
  struct FAT_DevInfo *devInfo;

  if( path == NULL || buffer == NULL )
    return -1;

  devInfo = lookupDeviceInfo( devNum );

  if( devInfo == NULL )
  {
    if( (devInfo = addDeviceInfo( devNum, 0 )) == NULL )
      return -1;
  }

  return readFile( path, offset, &devInfo->device, &devInfo->fatInfo, buffer, bytes );
}
*/
int fatGetAttributes( char *path, unsigned short devNum, char *buffer, size_t bufferLen, 
                      struct FileAttributes *attrib )
{
  struct FAT_DevInfo *devInfo;
  struct FAT_DirEntry entry;

  if( path == NULL || buffer == NULL || attrib == NULL )
    return -1;

  devInfo = lookupDeviceInfo( devNum );

  if( devInfo == NULL )
  {
    if( (devInfo = addDeviceInfo( devNum, 0 )) == NULL )
      return -1;
  }

  if( getFAT_DirEntry( path, devNum, devInfo, &entry ) != 0 )
    return -1;

  attrib->size = entry.file_size;
  attrib->nameLen = 0;

  for(int i=0; i < 8 && entry.filename[i] != ' '; i++, attrib->nameLen++)
    attrib->name[i] = entry.filename[i];

  if( entry.filename[8] != ' ' )
  {
    attrib->name[attrib->nameLen++] = '.';
    
    for( int i=8; i < 11 && entry.filename[attrib->nameLen-1] != ' '; )
      attrib->name[attrib->nameLen++] = entry.filename[i++];
  }

  if( entry.attrib & FAT_SUBDIR )
    attrib->flags |= FS_DIR;

  if( entry.attrib & FAT_HIDDEN )
    attrib->flags |= FS_HIDDEN;

  if( entry.attrib & FAT_RDONLY )
    attrib->flags |= FS_RDONLY;

  return 0;
}

struct FAT_DevInfo *lookupDeviceInfo( unsigned short devNum )
{
  struct FAT_DevInfo *ptr;

  for( ptr=fatDevList; ptr != NULL && ptr->fatInfo.deviceNum != devNum; ptr=ptr->next );

  return ptr;
}

struct FAT_DevInfo *addDeviceInfo( unsigned short devNum, int flags )
{
  int fatSize;
  struct FAT_DevInfo *fatDevInfo, *ptr;

  fatDevInfo = malloc( sizeof( struct FAT_DevInfo ) );

  if( fatDevInfo == NULL )
    return NULL;

  if( lookupDevMajor( MAJOR(devNum), &fatDevInfo->device ) != 0 )
  {
    free( fatDevInfo );
    return NULL;
  }

  /* Read the first sector and extract the boot parameter block. */

  deviceRead( fatDevInfo->device.ownerTID, MINOR(devNum), 0, 1, 512, &fatDevInfo->fatInfo.bpb );

  /* Fill up the FAT data cache(for speeding up calculations) */

  fatDevInfo->fatInfo.deviceNum = devNum;
  fatDevInfo->fatInfo.cache.fatSize = fatDevInfo->fatInfo.bpb.fat12.secs_per_fat;//CalcFatSize(bpb);
  fatDevInfo->fatInfo.cache.fatType = determineFAT_Type(&fatDevInfo->fatInfo.bpb);
  fatDevInfo->fatInfo.cache.clusterCount = calcClusCount(&fatDevInfo->fatInfo.bpb);
  fatDevInfo->fatInfo.cache.firstDataSec = firstDataSec(&fatDevInfo->fatInfo.bpb);
  fatDevInfo->fatInfo.cache.numDataSecs = calcDataSecs(&fatDevInfo->fatInfo.bpb);
  fatDevInfo->fatInfo.cache.rootDirSecs = calcRootDirSecs(&fatDevInfo->fatInfo.bpb);

  /* This may be wrong. */

  fatSize = fatDevInfo->fatInfo.cache.fatSize * fatDevInfo->device.dataBlkLen;

  /* Store a copy of the FAT into a buffer */

  if(fatDevInfo->fatInfo.cache.fatType == FAT12)
  {
    fatDevInfo->fatInfo.cache.table.fat12 = (word *)malloc(fatSize);

    for(int i=0; i < (fatSize /fatDevInfo->device.dataBlkLen); i++)
    {
      if( deviceRead(fatDevInfo->device.ownerTID, MINOR(devNum), i + 1, 1, fatDevInfo->device.dataBlkLen, fbuffer) < 0 )
      {
        printMsg("deviceRead(): Error\n");
        free( fatDevInfo->fatInfo.cache.table.fat12 );
        free( fatDevInfo );
        return NULL;
      }
      memcpy( (void *)((int)fatDevInfo->fatInfo.cache.table.fat12 +fatDevInfo->device.dataBlkLen * i), fbuffer,fatDevInfo->device.dataBlkLen );
    }
  }

  if( fatDevList == NULL )
    fatDevList = fatDevInfo;
  else
  {
    for(ptr=fatDevList; ptr->next != NULL; ptr=ptr->next);
    ptr->next = fatDevList;
  }

  return fatDevInfo;
}

/* FIXME: This needs to be fixed. It is VERY buggy! */

char *generate8_3Name( char *name, size_t nameLen, char *new_name ) /* Problem: readFile uses strtok() */
{
  char filename[11];
  int i;
  size_t extLen;

  if( name == NULL || new_name == NULL )
    return NULL;

//  if( strpbrk( name, " /" ) != NULL )
//    return NULL;
  /*else*/ if( nameLen == 0 || name[0] == '\0' )
    return NULL;

  if( strncmp(name, ".", nameLen) == 0 )
  {
    filename[0] = '.';
    memset( &filename[1], ' ', 10 );
  }
  else if( strncmp(name, "..", nameLen) == 0 )
  {
    filename[0] = filename[1] = '.';
    memset( &filename[2], ' ', 9 );
  }
  else
  {
//    size_t c_count=0;
    int k;

    for(k=0; k < nameLen; k++ )
    {
      if( name[k] == '.' )
        break;
    }

    if( /*strchr( name, '.', len ) == NULL*/k == nameLen )
    {
      if( k > 11 )
        return NULL;

      memcpy( filename, name, nameLen );
      memset( &filename[nameLen], ' ', 11-nameLen );
    }
    else
    {
      int j;

      for( j=0; j < nameLen; j++ )
      {
        if( name[j] == '.' )
          break;
      }

      if( j == 0 || j > 8 )
        return NULL;

      memcpy( filename, name, j );
      memset( &filename[j], ' ', 8 - j); // this doesn't work?

      extLen = nameLen-j-1;//strlen(&name[len+1]);

      if( extLen > 3 || extLen == 0 )
        return NULL;

      memcpy( &filename[8], &name[j+1], extLen );
      memset( &filename[8+extLen], ' ', 3-extLen );
    }
  }

  for(i=0; i < 11; i++)
    new_name[i] = (islower(filename[i]) ? toupper(filename[i]) : filename[i]) ;

  return new_name;
}
/* This may modify the path string. */

char *next_token( char *str, size_t *len )
{
  int i;

  if( str == NULL )
  {
    if( len != NULL )
      *len = 0;

    return NULL;
  }

  for(i=0; str[i] != '\0' && str[i] != '/'; i++);

  if( len != NULL )
    *len = i;

  if( str[i] == '\0' )
    return &str[i];
  else
    return &str[i+1];
}

int getFAT_DirEntry( char *path, struct Device *device, struct FAT_Dev *fat_dev,
                     struct FAT_DirEntry *dirEntry )
{
  int i;
  int startSector, clusterSize, dirSize, rootEnts, ret=-1;
  char *buffer, *nextToken, fileName[11], *retFname;
  struct FAT_DirEntry entry;
  size_t fnameLen;
  bool inRootDir=true;
  unsigned cluster=0;

  if( path == NULL || device == NULL || fat_dev == NULL || dirEntry == NULL )
  {
    printMsg("Null path, device, fat_dev, or dirEntry\n");
    return -1;
  }

  else if( path[0] != '/' )
  {
    printMsg("Code requires the pathname to start with a '/'");
    return -1;
  }

  if(fat_dev->cache.fatType == FAT12)
  {
    dirSize = fat_dev->bpb.fat12.bytes_per_sec;
    clusterSize = dirSize * fat_dev->bpb.fat12.secs_per_clus;
    rootEnts = fat_dev->bpb.fat12.root_ents;
    startSector = fat_dev->bpb.fat12.resd_secs + fat_dev->bpb.fat12.secs_per_fat * fat_dev->bpb.fat12.fat_copies;
  }
  else if(fat_dev->cache.fatType == FAT16)
  {
    dirSize = fat_dev->bpb.fat16.bytes_per_sec;
    clusterSize = dirSize * fat_dev->bpb.fat16.secs_per_clus;
    rootEnts = fat_dev->bpb.fat16.root_ents;
    startSector = fat_dev->bpb.fat16.resd_secs + fat_dev->bpb.fat16.secs_per_fat * fat_dev->bpb.fat16.fat_copies;
  }

  nextToken = next_token(&path[1], &fnameLen);
  retFname = generate8_3Name(&path[1], fnameLen, fileName );

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

      if( readSector( device, fat_dev, startSector++, buffer ) != 0 )
      {
        printMsg("readSector() failed.\n");
        break;
      }
    }
    else
    {
      if( getClusterType(cluster, fat_dev->cache.fatType) != USED_CLUSTER )
        break;

      if( readCluster( device, fat_dev, cluster, buffer ) != 0 ) /* Read in a cluster of directory entries */
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
        cluster = fat_dev->cache.fatType == FAT12 ? 0xFF8 : (fat_dev->cache.fatType == FAT16 ? 0xFFF8 : 0xFFFFFF8);
        break;
      }
      else if(entry.filename[0] == (byte)0xE5 || entry.start_clus < 2 ) /* Disregard deleted file entries */
        continue;
      else if( entry.filename[0] == 0x05 )
        entry.filename[0] = 0xe5;

      if( strncmp( (char *)entry.filename, fileName, 11 ) == 0 ) /* Found the directory entry */
      {
        cluster = entry.start_clus;

	next_token( nextToken, &fnameLen );
	retFname = generate8_3Name( nextToken, fnameLen, fileName );
        nextToken = &nextToken[fnameLen+1];

        if( retFname == NULL )
        {
          ret = 0;
          *dirEntry = entry;
          goto end_search;
        }
        else if( !(dirEntry->attrib & FAT_SUBDIR) )
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

    if( !inRootDir && i == dirSize / sizeof entry )
      cluster = readFAT( fat_dev, cluster );
  }

end_search:

  free( buffer );
  return ret;
}

int _readFile( tid_t tid, char *path, unsigned int offset, struct FAT_DevInfo *fatInfo, 
              size_t length )
{
  int written=0;
  char *buffer;
  unsigned cluster, clusterSize, clusterOffset;
  struct Device *device = fatInfo->device;
  struct FAT_Dev *fat_dev = fatInfo->fatDev;
  struct FAT_DirEntry entry;

  if( fatInfo == NULL )
    return -1;

  device = fatInfo->device;
  fat_dev = fatInfo->fatDev;

  if( getFAT_DirEntry( path, device, fat_dev, &entry ) != 0 )
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
    readCluster( device, fat_dev, cluster, buffer );

    if( length > (clusterSize - clusterOffset) && entry.file_size > clusterSize ) /* If file buffer and file entry is greater than the size of a whole cluster..., copy a whole cluster */
    {
      _send( tid, buffer + clusterOffset, clusterSize - clusterOffset, 0 );
      //memcpy( fileBuffer, buffer + clusterOffset, clusterSize - clusterOffset );
      //fileBuffer += (clusterSize - clusterOffset);
      length -= (clusterSize - clusterOffset);
      entry.file_size -= clusterSize;
      written += (clusterSize - clusterOffset);
    }
    else if( (length > (clusterSize - clusterOffset) && entry.file_size <= clusterSize) || 
             (length <= (clusterSize - clusterOffset) && entry.file_size <= clusterSize &&
               entry.file_size <= length) ) // If there's enough buffer space, but the remaining file data is less than the size of a cluster AND the remaining file data can fit in the remaining buffer space
    {
      _send( tid, buffer + clusterOffset, entry.file_size - clusterOffset );
      //memcpy( fileBuffer, buffer + clusterOffset, entry.file_size - clusterOffset );
      written += entry.file_size - clusterOffset;
      entry.file_size = 0;
      break;
    }
    else
    {
      _send( tid, buffer + clusterOffset, length );
      //memcpy( fileBuffer, buffer + clusterOffset, length );
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

/* This reads in an entire cluster from a device into a buffer.

 XXX: This implementation was just slapped together and needs work.
   Buffer must be at LEAST (bytesPerSector * sectorsPerCluster) long. It has to be able
   to hold an entire cluster. */

int readCluster( struct Device *device, struct FAT_Dev *fat_dev, unsigned cluster, void *buffer )
{
  unsigned blocksPerSector, sectorsPerCluster, bytes_per_sec;
  unsigned blockStart;
  enum FAT_ClusterType clusterType;

  if( device == NULL || fat_dev == NULL || buffer == NULL )
    return -1;

  clusterType = getClusterType( cluster, fat_dev->cache.fatType );

  /* Why would you ever want to read a free cluster? */

  if( clusterType != USED_CLUSTER && clusterType != FREE_CLUSTER )
  {
    return -1;
  }

  if(fat_dev->cache.fatType == FAT12)
  {
    bytes_per_sec = fat_dev->bpb.fat12.bytes_per_sec;
    sectorsPerCluster = fat_dev->bpb.fat12.secs_per_clus;
  }
  else if(fat_dev->cache.fatType == FAT16)
  {
    bytes_per_sec = fat_dev->bpb.fat16.bytes_per_sec;
    sectorsPerCluster = fat_dev->bpb.fat16.secs_per_clus;
  }
  else
    return 1;

  blocksPerSector = bytes_per_sec / device->dataBlkLen; // Flawed

  blockStart = (fat_dev->cache.firstDataSec + (cluster-2) * sectorsPerCluster) * blocksPerSector;

  if( deviceRead( device->ownerTID, MINOR(fat_dev->deviceNum), blockStart, blocksPerSector * sectorsPerCluster,
                  bytes_per_sec * sectorsPerCluster, buffer ) == -1 )
  {
    printMsg("deviceRead() fail\n");
    return -1;
  }
  return 0;
}

int readSector( struct Device *device, struct FAT_Dev *fat_dev, unsigned sector, void *buffer )
{
  unsigned blocksPerSector, bytes_per_sec;
  unsigned blockStart;

  if( device == NULL || fat_dev == NULL || buffer == NULL )
    return -1;

  blocksPerSector = device->dataBlkLen;

  if(fat_dev->cache.fatType == FAT12)
    bytes_per_sec = fat_dev->bpb.fat12.bytes_per_sec;
  else if(fat_dev->cache.fatType == FAT16)
    bytes_per_sec = fat_dev->bpb.fat16.bytes_per_sec;
  else
    return 1;

  blocksPerSector = bytes_per_sec / device->dataBlkLen; // Flawed

  blockStart = sector * blocksPerSector;

  if( deviceRead( device->ownerTID, MINOR(fat_dev->deviceNum), blockStart, blocksPerSector,
                  bytes_per_sec, buffer ) == -1 )
  {
    printMsg("deviceRead() fail\n");
    return -1;
  }
  return 0;
}

/* XXX: To be implemented...

int getDirList( tid_t tid, char *path, struct FAT_DevInfo *fatInfo,
                size_t maxEntries )
{
  struct FAT_DirEntry *entry, directory;
  int i;
  enum FAT_Type type;
  unsigned clusterSize, dirSize, cluster, sector, numRootEnts;
  char *buffer, *pathName;
  bool isRootDir = false, done=false;
  int numEntries = 0, nameLen, extLen;
  struct FileAttributes *fsEntry;
  struct Device *device;
  struct FAT_Dev *fat_dev;

  if( path == NULL || fatInfo == NULL )
    return -1;

  fat_dev = fatInfo->fat_dev;
  device = fatInfo->device;

  if( strcmp( path, "/" ) == 0 )
  {
    isRootDir = true;
    type = determineFAT_Type( &fat_dev->bpb );

    if( type == FAT12 )
    {
      dirSize = fat_dev->bpb.fat12.bytes_per_sec;
      numRootEnts = fat_dev->bpb.fat12.root_ents;
      sector = fat_dev->bpb.fat12.resd_secs + fat_dev->bpb.fat12.secs_per_fat * 
               fat_dev->bpb.fat12.fat_copies;
    }
    else if( type == FAT16 )
    {
      dirSize = fat_dev->bpb.fat16.bytes_per_sec;
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
    if( getFAT_DirEntry( path, device, fat_dev, &directory ) || 
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

  fsEntry = (struct FileAttributes *)file_buffer;

  while( !done )
  {
    if( isRootDir )
    {
      if( numRootEnts == 0 )
      {
        done = true;
        break;
      }

      if( readSector( device, fat_dev, sector++, buffer ) != 0 )
      {
        printMsg("readSector() failed.\n");
        free( buffer );
        return -1;
      }
    }
    else
    {
      enum FAT_ClusterType clusterType = 
        getClusterType(cluster, fat_dev->cache.fatType);

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

      if( readCluster( device, fat_dev, cluster, buffer ) != 0 )
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
      strncpy( fsEntry->name, entry->filename, nameLen );

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
          strncpy( &fsEntry->name[nameLen+1], &entry->filename[8], extLen );
          fsEntry->nameLen += extLen + 1;
        }
      }

      maxEntries--;

      //fsEntry = (struct FileAttributes *)((char *)fsEntry + fsEntry->entryLen);
      fsEntry++; // send here

      if( fsEntry - (struct FileAttributes *)file_buffer >= 
            FBUFFER_LEN / sizeof(struct FileAttributes) )
      {
        _send( tid, file_buffer, FBUFFER_LEN / sizeof(struct FileAttributes), 0 );
        fsEntry = (struct FileAttributes *)file_buffer;
      }

      numEntries++;
    }

    if( !isRootDir )
      cluster = readFAT( fat_dev, cluster );
  }

  free(buffer);

  return numEntries;
}
*/

/*
int fat_init(struct vdir *root, int device, int flags)
{
  struct buffer *b;
  struct FAT12_BPB *bpb;
  struct FAT_Dev *dev;
  int i, fat_size;
  struct FS_header *fs_hdr, *header;

  bnew(device, 0);

  if(bnew == NULL)
    return -1;

  dev = (struct FAT_Dev *)kmalloc(sizeof(struct FAT_Dev));
  b = (struct buffer *)bread(device, 0); // Read the first block(sector)
  bpb = (struct FAT12_BPB *)b->data; // Extract the Boot Parameter Block
  
  / * Add the FS header to the header list. * /

  root->fs = (struct FS_header *)kmalloc(sizeof(struct FS_header));
  if(root->fs == NULL) {
//    kfree(root)
    return -1;
  }

  root->fs->next = NULL;
  
  if(fsheader_list == NULL)
    fsheader_list = root->fs;
  else {
    for(fs_hdr=fsheader_list; fs_hdr->next != NULL; fs_hdr = fs_hdr->next);
    fs_hdr->next = root->fs;
  }
  header = root->fs;
  
  / * Fill up the FS specific data struct * /

  memcpy(&dev->info, bpb, 512);
  header->device = device;
  header->blk_size = blk_dev[MAJOR(device)].blk_size;//bpb->bytes_per_sec * bpb->secs_per_clus;
  header->flags |= flags;

  dev->cache.fat_size = bpb->secs_per_fat;//CalcFatSize(bpb);
  dev->cache.fat_type = DetermineFAT_Type(bpb);
  dev->cache.clus_count = CalcClusCount(bpb);
  dev->cache.first_dat_sec = FirstDataSec(bpb);
  dev->cache.num_dat_secs = CalcDataSecs(bpb);
  dev->cache.root_dir_secs = CalcRootDirSecs(bpb);

  header->blk_count = dev->cache.clus_count;
  header->fs_info = (void *)dev;

  root->fs_info = (void *)dev;
  root->fs->fs_info = (void *)dev;
  root->fs->f_ops = &fat_fops;
  root->fs->d_ops = &fat_dops;

  root->attrib = 0;

  / * Store a copy of the FAT in the cache. * /

  fat_size = dev->cache.fat_size * header->blk_size;
  
  if(dev->cache.fat_type == 0) { // FAT12
    dev->cache.table.fat12 = (word *)kmalloc(fat_size);
    
    for(i=0; i < (fat_size / header->blk_size); i++) {
      bnew(device, i + 1);
      bread(device, i + 1);
      memcpy((void *)((addr_t)dev->cache.table.fat12 + header->blk_size * i), 
              b->data, header->blk_size);
    }
  }
  else if(dev->cache.fat_type == 1) { // FAT16
    dev->cache.table.fat16 = (word *)kmalloc(fat_size);
    
    for(i=0; i < (fat_size / header->blk_size); i++) {
      bnew(device, i + 1);
      bread(device, i + 1);
      memcpy((void *)((addr_t)dev->cache.table.fat16 + header->blk_size * i),
              b->data, header->blk_size);
    }
  }
//  else if(type == 2) {} FAT32
  
  root->blk_count = 1;
  root->blk_size = 512;
  root->blk_start = (fat_size / header->blk_size) * dev->info.fat16.fat_copies + 1;

  return 0;
} 

int mod_main(void)
{ 
  struct filesystem fs = { "fat", fat_init, 0 };

  RegisterFS(&fs);
  printMsg("FAT filesystem registered.\n");
//  Mount("fat", MAKE_DEVNUM(3, 0), "adrive", FL_DIRRONLY);
//  while(1);
}
*/

char someBuffer[1024];
/*
int main(void)
{
  int entries;
  struct DirEntry *entry;
  int i;

  __sleep(5000);
  printMsg("Init FAT\n");

  if( addDeviceInfo( 0x0a00, 0 ) < 0 )
    printMsg("Error.\n");

  fatReadFile( "/regs.txt", 0x0a00, 0, sizeof someBuffer, someBuffer );
/ *
  entries = fatGetDirList( "/system/../system/servers", 0x0a00, someBuffer, sizeof someBuffer );
  entry = (struct DirEntry *)someBuffer;

  for( i=0; i < entries; i++ )
  {
    printMsgN( entry->fileName, entry->entryLen - sizeof( struct DirEntry) );
    printMsg("\n");
    entry = (struct DirEntry *)((char *)entry + entry->entryLen);
  }
* /
 // load_elf_exec( "/system/servers/keyboard.exe", 0x0a00 int readFile( char *path, struct Device *device, struct FAT_Dev *fat_dev, 
              char *fileBuffer, int fBufferLen ));

  printMsg( someBuffer );
  __pause();
  return 0;
}
*/

#define FBUFFER_LEN	65536

char *file_buffer = NULL;

void handle_error( volatile struct Message *msg );

void handle_create_dir( volatile struct Message *msg )
{
  handle_error(msg);
}

void handle_create_file( volatile struct Message *msg )
{
  handle_error(msg);
}

void handle_write_file( volatile struct Message *msg )
{
  handle_error(msg);
}

//int fatReadFile( char *path, unsigned short devNum, unsigned int offset, size_t bytes, 
//                 char *buffer );

void handle_read_file( volatile struct Message *msg )
{
  volatile struct FsReqMsg *req = (volatile struct FsReqMsg *)msg->data;
  volatile struct FsReplyMsg *reply = (volatile struct FsReqMsg *)msg->data;
  size_t num_bytes = req->dataLen;
  tid_t tid = msg->sender;
  unsigned char pathLen = req->pathLen;
  size_t offset = req->offset;
  struct FAT_DevInfo *fatDev = lookupDeviceInfo(req->devNum);
  char *path;
  size_t num_sent=0;
  int ret;
  unsigned short devNum = req->devNum;

  if( fatDev == NULL )
  {
    if( (fatDev = addDeviceInfo( req->devNum, 0 )) == NULL )
      return;
  }

  if( pathLen == 0 || (path=malloc(pathLen+1)) == NULL )
  {
    reply->fail = 1;
    reply->bufferLen = 0;

    while( (ret=__send( tid, (struct Message *)&msg, 0 )) == 2 );
    return;
  }

  path[pathLen] = '\0';

  reply->fail = 0;
  reply->bufferLen = FBUFFER_LEN;

  while( (ret=__send( tid, &msg, 0 )) == 2 );

  if( ret < 0 )
    return -1;

  size_t pathOffset = 0;

  while( pathOffset < pathLen )
  {
    _receive( tid, path + pathOffset, pathLen - pathOffset > FBUFFER_LEN ? 
      FBUFFER_LEN : pathLen - pathOffset, 0 );
    pathOffset += pathLen - pathOffset > FBUFFER_LEN ? FBUFFER_LEN : pathLen - pathOffset;
  }

  if( _readFile( tid, path, offset, fatDev, num_bytes ) < 0 )
  {
    print("_readFile() failed\n");
  }
/*
  while( num_sent < num_bytes )
  {
    size_t diff = (num_bytes - num_sent);
    size_t n;

    n = readFile( path, offset, &fatDev->device, &fatDev->fatInfo, file_buffer, 
                    diff > FBUFFER_LEN ? FBUFFER_LEN : num_bytes );

    if( n < 0 )
    {
      print("FIXME: Fatal error! fatReadFile() failed!\n");
      return;
    }

    offset += n;
    num_sent += _send( tid, file_buffer, n, 0 );
  }*/
}

void handle_get_attrib( volatile struct Message *msg )
{

}

void handle_set_attrib( volatile struct Message *msg )
{
  handle_error(msg);
}

void handle_remove( volatile struct Message *msg )
{
  handle_error(msg);  
}

void handle_link( volatile struct Message *msg )
{
  handle_error(msg);
}

void handle_unlink( volatile struct Message *msg )
{
  handle_error(msg);
}

void handle_list_dir( volatile struct Message *msg )
{

}

void handle_fsctl( volatile struct Message *msg )
{
  handle_error(msg);
}

void handle_error( volatile struct Message *msg )
{

}

void handleFsRequests( void )
{
  volatile struct Message msg;
  volatile struct FsReqMsg *req = (volatile struct FsReqMsg *)msg.data;

  while( __receive(NULL_TID, &msg, 0) == 2 );

  if( msg.protocol == MSG_PROTO_FS )
  {
    switch(req->req)
    {
      case CREATE_DIR:
        handle_create_dir(&msg);
        break;
      case CREATE_FILE:
        handle_create_file(&msg);
        break;
      case WRITE_FILE:
        handle_write_file(&msg);
        break;
      case READ_FILE:
        handle_read_file(&msg);
        break;
      case FSCTL:
        handle_fsctl(&msg);
        break;
      case GET_ATTRIB:
        handle_get_attrib(&msg);
        break;
      case SET_ATTRIB:
        handle_set_attrib(&msg);
        break;
      case LIST_DIR:
        handle_list_dir(&msg);
        break;
      case REMOVE:
        handle_remove(&msg);
        break;
      case LINK:
        handle_link(&msg);
        break;
      case UNLINK:
        handle_unlink(&msg);
        break;
      default:
        handle_fs_error(&msg);
        break;
    }
  }
}

int main(void)
{
  struct Filesystem fsInfo;
  int status;

  //printMsg("Starting Ramdisk\n");

  __sleep(5000);
  printMsg("Init FAT\n");

  file_buffer = malloc(FBUFFER_LEN);

  for( int i=0; i < 5; i++ )
  {
    status = registerName(SERVER_NAME, strlen(SERVER_NAME));      

    if( status != 0 )
      __sleep( (i*i+1) * 500 );
    else
      break;
  }

  if( status != 0 )
    return 1;
/*
  devInfo.cacheType = NO_CACHE;
  devInfo.type = BLOCK_DEV;
  devInfo.dataBlkLen = RAMDISK_BLKSIZE;
  devInfo.numDevices = NUM_RAMDISKS;
  devInfo.major = RAMDISK_MAJOR;
*/

  fsInfo.flags = 0;

  for( int i=0; i < 5; i++ )
  {
    status = registerFs(FS_NAME, strlen(FS_NAME), &fsInfo);

    if( status != 0 )
      __sleep( (i*i+1) * 500 );
    else
      break;
  }

  if( status != 0 )
    return 1;

  while(1)
    handleFsRequests();

  return 1;
}
