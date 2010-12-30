#include <oslib.h>
#include <os/message.h>
#include <os/os_types.h>
#include <os/vfs.h>
#include <string.h>
#include "../name.h"

/* The VFS is in charge of keeping a record of all registered filesystems. */

// 1. Filesystem list (filesystem, server)
// 2. Mount Table (filesystem, virtual path, device, flags)



/* The VFS will appear to act as a single, global filesystem while it translates VFS requests into filesystem specific requests (for FAT, ext2, ntfs, etc.).*/

/*
  statuses:
    Path not found
    Bad parameter(s)
    Operation Failed
    File exists

Uniform interface with other filesystems. These are, essentially, wrapper
 functions that will simply forward requests to the respective filesystem.

  enum FsRequest { CREATE_DIR, LIST, CREATE_FILE, READ, WRITE,
                   REMOVE, LINK, UNLINK, GET_ATTRIB, SET_ATTRIB, FSCTL,
                   MOUNT, UNMOUNT };

  int createDir( char *name, size_t name_len, char *path, size_t path_len );
  int list( char *path, size_t path_len );
  int createFile( char *name, size_t name_len, char *path, size_t path_len );
  int read( char *path, size_t path_len );
  int write( char *path, size_t path_len, char *buffer, size_t buflen );
  int remove( char *path, size_t path_len );
  int link( char *name, size_t name_len, char *path, size_t path_len ); // Creates a hard link
  int unlink( char *path, size_t path_len );
  int getAttributes( char *path, size_t path_len );
  int setAttributes( char *path, size_t path_len, struct FileAttributes attrib );
  int fsctl( int operation, ... );

  int mount( int device, char *filesystem, size_t fsLen, char *path, size_t pathLen );
  int unmount( char *path, size_t path_len );

  Flags/Attributes:
    *Flags: Hidden, locked, directory, link, virtual, deleted, read-only,
      online, compressed, encrypted (32-bit)
    *Timestamp (64-bit)
    *Size (64-bit)
    *Name (255 bytes)
    *Name length (1 byte)

  #define FS_DIR	0x001
  #define FS_HIDDEN	0x002
  #define FS_LOCKED	0x004
  #define FS_LINK	0x008
  #define FS_VIRTUAL	0x010
  #define FS_DELETED	0x020
  #define FS_RDONLY	0x040
  #define FS_ONLINE	0x080
  #define FS_COMPRESS	0x100
  #define FS_ENCRYPT	0x200

  struct FileAttributes
  {
    int flags;
    long long timestamp; // number of microseconds since Jan. 1, 1950 CE
    long long size;
    unsigned char name_len;
    char name[255];
  };

  struct MountEntry
  {
    int device;
    SBString fs;
    SBString path;
    int flags;
  };

  struct FsReqMsgR
  {
    int req;
    SBString path;
    size_t dataLen;

    char data[];
  };
*/

/* SBFilePath is simply an SBArray of SBStrings. Each SBString
   is a directory. The first SBString is the directory in the
   root directory. The subsequent directories are subdirectories. */


/*
struct FileAttributes
{
  int flags;
  long long timestamp; // number of microseconds since Jan. 1, 1950 CE
  long long size;
  unsigned char name_len;
  char name[255];
};

struct FSOps
{
  int (*createDir)( SBString *, SBFilePath * );
  int (*list)( SBFilePath *, struct FileAttributes ** );
  int (*createFile)( SBString *, SBFilePath * );
  int (*read)( SBFilePath *, char **, size_t );
  int (*write)( SBFilePath *, char *, size_t );
  int (*remove)( SBFilePath * );
  int (*link)( SBString *, SBFilePath * ); // Creates a hard link
  int (*unlink)( SBFilePath * );
  int (*getAttributes)( SBFilePath *, struct FileAttributes ** );
  int (*setAttributes)( SBFilePath *, struct FileAttributes * );
  int (*fsctl)( int, char *, size_t, char **, size_t * );
};

struct MountEntry
{
  int device;
  struct Filesystem *fs;
  SBString path;
  int flags;
};

struct Filesystem
{
  char name[12];
  struct FSOps fsOps;
};
*/

SBAssocArray mountTable;

enum SBFilePathStatus { SBFilePathError=-1, SBFilePathFailed=-2 };

// path(sbString) -> MountEntry

int sbFilePathAtLevel(const SBFilePath *path, int level, SBString *str);
int sbFilePathCreate(SBFilePath *path);
int sbFilePathDelete(SBFilePath *path);
int sbFilePathDepth(const SBFilePath *path, size_t *depth);
int stringToPath(const SBString *string, SBFilePath *path);
static int lookupMountEntry(const SBString *path, struct MountEntry **entry,
    SBFilePath *relPath);
static int mount( int device, const char *fsName, size_t fsNameLen, 
    const SBString *path, int flags );
static int unmount( const SBString *path );
int handleVfsRequest(tid_t sender, struct FsReqHeader *req, char *inBuffer, 
size_t inBytes, char **outBuffer, size_t *outBytes);

void printFilePath(const SBFilePath *path);

void printFilePath(const SBFilePath *path)
{
  SBString *tStr;

  print("'");
  if( path->nElems == 0 )
    print("/");

  for(int i=0; i < path->nElems; i++)
  {
    if( sbArrayElemAt(path, i, (void **)&tStr, NULL) != 0 )
    {
      print("unable to print path\n");
      return;
    }
    print("/");
    printN(tStr->data, sbStringLength(tStr));
  }
  print("'");
}

int sbFilePathAtLevel(const SBFilePath *path, int level, SBString *str)
{
  SBString *tStr;

  if( !path )
    return SBFilePathError;

  if( level == 0 && path->nElems == 0 )
  {
    sbStringCreate(str, NULL);
    return 0;
  }

  if( level >= path->nElems )
    return SBFilePathFailed;

  if( sbArrayElemAt(path, level, (void **)&tStr, NULL) != 0 )
    return SBFilePathFailed;

  if( sbStringCopy(tStr, str) < 0 )
    return -1;
  else
    return 0;
}

int sbFilePathCreate(SBFilePath *path)
{
  return sbArrayCreate(path);
}

int sbFilePathDelete(SBFilePath *path)
{
  SBString *str;

  if( !path )
    return -1;

  while( sbArrayCount(path) )
  {
    sbArrayPop(path, (void **)&str, NULL);
    sbStringDelete(str);
  }

  return 0;
}

int sbFilePathDepth(const SBFilePath *path, size_t *depth)
{
  if( !path )
    return SBFilePathError;

  if( depth )
    *depth = path->nElems;

  return 0;
}

int stringToPath(const SBString *string, SBFilePath *path)
{
  SBString separator, name;
  SBFilePath tmpPath;
  int retcode;
  int depth;
  int i=0;

  if( !string || !path || sbStringCreate(&separator, "/") < 0 )
    return -1;

  retcode = (sbStringSplit(string, &separator,-1, path) != 0 ? -1 : 0);
  sbStringDelete(&separator);

  if( retcode == -1 )
    return -1;

  if( sbFilePathDepth(path, &depth) != 0 )
  {
    sbFilePathDelete(path);
    return -1;
  }

  while( depth-- )
  {
    sbFilePathAtLevel(path, i, &name);

    if( sbStringLength(&name) == 0 )
      sbArrayRemove(path, i);
    else
      i++;
  }

  sbFilePathDepth(path, &depth);

  return 0;
}

static int lookupMountEntry(const SBString *path, struct MountEntry **entry,
    SBFilePath *relPath)
{
  SBFilePath fPath;
  SBFilePath tmpPath;
  SBKey *keys, *bestKey=NULL;
  size_t numKeys=0, bestLength=0;

  if( path == NULL )
    return -1;

  sbFilePathCreate(&fPath);

  if( stringToPath(path, &fPath) != 0 )
  {
    sbFilePathDelete(&fPath);
    return -1;
  }

  if( sbAssocArrayKeys(&mountTable, &keys, &numKeys ) != 0 )
  {
    sbFilePathDelete(&fPath);
    return -1;
  }

  for(unsigned i=0; i < numKeys; i++, keys++)
  {
    size_t pathDepth, tmpDepth;
    SBString subPath, subTmp, keyStr;

    if( sbStringCreateN(&keyStr, keys->key, keys->keysize) < 0 )
      continue;

    sbFilePathCreate(&tmpPath);

    if( stringToPath(&keyStr, &tmpPath) != 0 )
    {
      sbFilePathDelete(&tmpPath);
      continue;
    }

    sbFilePathDepth(&fPath, &pathDepth);
    sbFilePathDepth(&tmpPath, &tmpDepth);

    if( pathDepth < tmpDepth )
    {
      sbFilePathDelete(&tmpPath);
      continue;
    }
    else if( tmpDepth == 0 )
    {
      SBString rootPath;

      if( sbStringCreate(&rootPath, "/") < 0 )
      {
        sbFilePathDelete(&fPath);
        sbFilePathDelete(&tmpPath);
        return -1;
      }

      if( entry )
      {
        if( sbAssocArrayLookup(&mountTable, rootPath.data, sbStringLength(&rootPath), (void **)entry, NULL) != 0 )
        {
          sbFilePathDelete(&fPath);
          sbFilePathDelete(&tmpPath);
          sbStringDelete(&rootPath);
          return -1;
        }
      }

      if( relPath )
        stringToPath(path, relPath);

      sbStringDelete(&rootPath);
      sbFilePathDelete(&tmpPath);
      sbFilePathDelete(&fPath);
      return 0;
    }

    for(unsigned j=0; j < tmpDepth; j++)
    {
      if( sbFilePathAtLevel(&fPath, j, &subPath) != 0 )
        break;
      if( sbFilePathAtLevel(&tmpPath, j, &subTmp) != 0 )
        break;
      if( sbStringCompare(&subPath, &subTmp) != SBStringCompareEqual )
        break;

      if( j + 1 > bestLength )
      {
        bestLength = j+1;
        bestKey = keys;
      }
    }

    sbFilePathDelete(&tmpPath);
  }

  if( !bestKey )
  {
    print("No best key.\n");
    return -1;
  }

  if( entry )
  {
    if( sbAssocArrayLookup(&mountTable, bestKey->key, bestKey->keysize, (void **)entry, NULL) != 0 )
    {
      sbFilePathDelete(&fPath);
      return -1;
    }
  }

  if( relPath && sbArraySlice(&fPath, bestLength, -1, relPath) != 0 )
  {
    sbFilePathDelete(&fPath);
    return -1;
  }

  sbFilePathDelete(&fPath);
  return 0;
}

/* The requests

int createDir( SBString *name, SBFilePath *path );
int list( SBFilePath *path, struct FileAttributes *fileList );
int createFile( SBString *name, SBFilePath *path );
int read( SBFilePath *path, size_t numBytes );
int write( SBFilePath *path, char *buffer, size_t numBytes );
int remove( SBFilePath *path );
int link( SBString *name, SBFilePath *path ); // Creates a hard link
int unlink( SBFilePath *path );
int getAttributes( SBFilePath *path, struct FileAttributes *attrib );
int setAttributes( SBFilePath *path, struct FileAttributes *attrib );
int fsctl( int operation, char *buffer, size_t argLen );
*/

static int mount( int device, const char *fsName, size_t fsNameLen,
  const SBString *path, int flags )
{
  struct MountEntry *entry;
  struct NameRecord *record;

  if( fsNameLen > VFS_NAME_MAXLEN || fsNameLen == 0 )
    return MountError;

  if( sbAssocArrayLookup(&mountTable, (void *)path->data, sbStringLength(path), NULL, NULL) != SBAssocArrayNotFound )
    return MountExists;

  if( path )
  {
    record = _lookupName(fsName, fsNameLen, FS);

    if( !record )
      return MountFailed;

    entry = malloc(sizeof *entry);

    if( !entry )
      return MountFailed;

    if( sbStringCopy(path, &entry->path) < 0 )
    {
      free(entry);
      return MountFailed;
    }

    entry->fs = &record->entry.fs;
    entry->device = device;
    entry->flags = flags;
  }
  else
    return MountError;

  if( sbAssocArrayInsert(&mountTable, (void *)path->data, sbStringLength(path), 
        entry, sizeof(entry)) != 0 )
  {
    sbStringDelete(&entry->path);
    free(entry);
    return MountFailed;
  }

  return 0;
}

static int unmount( const SBString *path )
{
  struct MountEntry *entry=NULL;

  if( !path )
    return UnmountError;

  if( sbAssocArrayRemove(&mountTable, (void *)path->data, sbStringLength(path), (void **)&entry, NULL) )
  {
    if( sbAssocArrayLookup(&mountTable, (void *)path->data, sbStringLength(path), NULL, NULL) )
      return UnmountNotExist;
    else
      return UnmountFailed;
  }

  sbStringDelete(&entry->path);

  free(entry);
  return 0;
}

int handleVfsRequest(tid_t sender, struct FsReqHeader *req, char *inBuffer, 
size_t inBytes, char **outBuffer, size_t *outBytes)
{
  SBString path, name;
  SBFilePath fPath;
  struct FSOps *ops;
  struct MountEntry *entry;
  char *argStart = inBuffer + req->pathLen;
  int ret = -1;

  sbStringCreate(&name, NULL);
  sbStringCreate(&path, NULL);
  sbFilePathCreate(&fPath);

  // FIXME: ops is determined by the mount point in the path
  // To lookup mount point, start from the entire path and work
  // your way up toward the root directory matching entries in the
  // mount table. Stop on the first match. If no matches, then use the root
  // directory (if in table)

  *outBuffer = NULL;
  *outBytes = 0;
/*
  print("Request: 0x");
  printHex(req->request);
  print("\n");
*/
  if( req->request != FSCTL )
  {
    if( sbStringCreateN(&path, inBuffer, req->pathLen) < 0 )
    {
      ret = -1;
      goto vfsReturn;
    }

    if( req->request != MOUNT && req->request != UNMOUNT )
    {
      if( lookupMountEntry(&path, &entry, &fPath) != 0 )
      {
        ret = -1;
        goto vfsReturn;
      }
      ops = &entry->fs->fsOps;
    }

    switch(req->request)
    {
      case CREATE_DIR:
      case CREATE_FILE:
      case LINK:
        if( sbStringCreateN(&name, argStart, req->argLen) < 0 )
        {
          ret = -1;
          goto vfsReturn;
        }
      default:
        break;
    }
  }

  switch(req->request)
  {
    case CREATE_DIR:
      ret = ops->createDir(&name, &fPath);
      break;
    case LIST:
    {
      struct VfsListArgs *args = (struct VfsListArgs *)argStart;
      ret = ops->list(entry->device, &fPath, args, (struct FileAttributes **)outBuffer);
       *outBytes = (ret < 0 ? 0 : ret * sizeof(struct FileAttributes));
      break;
    }
    case CREATE_FILE:
      ret = ops->createFile(&name, &fPath);
      break;
    case READ:
    {
      struct VfsReadArgs *args = (struct VfsReadArgs *)argStart;
      ret = ops->read(entry->device, &fPath, args, outBuffer);
      *outBytes = (ret < 0 ? 0 : ret);
      break;
    }
    case WRITE:
      ret = ops->write(&fPath, argStart, inBytes);
      break;
    case REMOVE:
      ret = ops->remove(&fPath);
      break;
    case LINK:
      ret = ops->link(&name, &fPath);
      break;
    case UNLINK:
      ret = ops->unlink(&fPath);
      break;
    case GET_ATTRIB:
    {
      struct VfsGetAttribArgs *args = (struct VfsGetAttribArgs *)argStart;
      ret = ops->getAttributes(entry->device, &fPath, args, (struct FileAttributes **)outBuffer);
      *outBytes = (ret < 0 ? 0 : sizeof(struct FileAttributes));
      break;
    }
    case SET_ATTRIB:
      ret = ops->setAttributes(&fPath, (struct FileAttributes *)argStart);
      break;
    case FSCTL:
      //ret = vfsFsctl(req->operation, argStart, inBytes, outBuffer, outBytes);
      break;
    case MOUNT:
    {
      struct MountArgs *args = (struct MountArgs *)argStart;
      ret = mount(args->device, args->fs, args->fsLen, &path, args->flags);
      break;
    }
    case UNMOUNT:
      ret = unmount(&path);
      break;
    default:
      ret = -1;
      break;
  }

vfsReturn:
  sbStringDelete(&path);
  sbStringDelete(&name);
  sbFilePathDelete(&fPath);

  return ret;
}
/*
int main(void)
{
  volatile struct Message msg;
  volatile struct FsReqHeader *req = (volatile struct FsReqHeader *)msg.data;
  struct FsReplyHeader header;
  char *outBuffer, *sendBuffer, *inBuffer;
  size_t argLen;

  for( int i=0; i < 5; i++ )
  {
    int status = registerName(VFS_NAME, strlen(VFS_NAME));
    
    if( status != 0 )
      __sleep( (i*i+1) * 500 );
    else
      break;
  }

  printf("vfs registered\n");

  while(1)
  {
    while( __receive(NULL_TID, (struct Message *)&msg, 0) == 2 );

    if( msg.protocol == MSG_PROTO_VFS )
    {
      printf("received msg\n");
      inBuffer = malloc(req->argLen);

/ *
// XXX: need to reimplement this
      if( inBuffer )
        while( _receive(msg.sender, inBuffer, req->argLen, 0) == 2 );
* /
      header.reply = handleVfsRequest((struct FsReqHeader *)req, inBuffer, 
                       req->argLen, &outBuffer, &argLen);
      header.argLen = argLen;
      sendBuffer = malloc(sizeof header + argLen);

      if( !sendBuffer )
      {
        free(outBuffer);
        outBuffer = NULL;
        header.reply = -1;
        header.argLen = 0;

        while( _send(msg.sender, &header, sizeof header, 0) == 2 );
      }
      else
      {
/ *
        *(struct FsReplyHeader *)sendBuffer = header;
        memcpy((struct FsReplyHeader *)sendBuffer + 1, outBuffer, argLen);
* /
        while( __send(msg.sender, (struct Message *)&msg, 0) == 2 );

//        while( _send(msg.sender, sendBuffer, sizeof header + argLen, 0) == 2 );
      }

      free(outBuffer);
      free(sendBuffer);
    }
    else
      printf("vfs: Unrecognized protocol\n");
  }
  return 1;
}
*/
