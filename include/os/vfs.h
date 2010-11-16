#ifndef VIRTUAL_FS
#define VIRTUAL_FS

#include <types.h>
#include <os/file.h>
#include <os/os_types.h>
//#include <time.h>

#define VFS_NAME_MAXLEN		12

typedef SBArray SBFilePath;

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
  struct VFS_Filesystem *fs;
  SBString path;
  int flags;
};

struct VFS_Filesystem
{
  char name[VFS_NAME_MAXLEN];
  size_t nameLen;
  struct FSOps fsOps;
};

enum MountStatus { MountError=-1, MountFailed=-2, MountExists=-3 };
enum UnmountStatus { UnmountError=-1, UnmountFailed=-2, UnmountNotExist=-3 };

enum FsReqCode { CREATE_DIR, LIST, CREATE_FILE, READ, WRITE,
                   REMOVE, LINK, UNLINK, GET_ATTRIB, SET_ATTRIB, FSCTL,
                   MOUNT, UNMOUNT };

struct FsReqHeader
{
  enum FsReqCode request;

  union
  {
    size_t pathLen;
    int operation;
  };

  size_t argLen;
  char data[];
};

struct FsReplyHeader
{
  int reply;
  size_t argLen;
  char data[];
};

struct MountArgs
{
  int device;
  size_t fsLen;
  char fs[VFS_NAME_MAXLEN];
  int flags;
};

#endif
