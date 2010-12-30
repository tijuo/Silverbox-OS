#ifndef VIRTUAL_FS
#define VIRTUAL_FS

#include <types.h>
#include <os/file.h>
#include <os/os_types.h>
//#include <time.h>

#define VFS_NAME_MAXLEN		12

#define FS_DIR		0x001
#define FS_HIDDEN	0x002
#define FS_LOCKED	0x004
#define FS_LINK		0x008
#define FS_VIRTUAL	0x010
#define FS_DELETED	0x020
#define FS_RDONLY	0x040
#define FS_ONLINE	0x080
#define FS_COMPRESS	0x100
#define FS_ENCRYPT	0x200

typedef SBArray SBFilePath;

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
  struct VFS_Filesystem *fs;
  SBString path;
  int flags;
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

struct VfsListArgs
{
  size_t maxEntries;
};

struct VfsGetAttribArgs
{
  int _resd; // dummy value
};

struct VfsReadArgs
{
  int offset;
  size_t length;
};

struct MountArgs
{
  int device;
  size_t fsLen;
  char fs[VFS_NAME_MAXLEN];
  int flags;
};

struct FSOps
{
  int (*createDir)( SBString *, SBFilePath * );
  int (*list)( unsigned short, SBFilePath *, struct VfsListArgs *, struct FileAttributes ** );
  int (*createFile)( SBString *, SBFilePath * );
  int (*read)( unsigned short, SBFilePath *, struct VfsReadArgs *, char ** );
  int (*write)( SBFilePath *, char *, size_t );
  int (*remove)( SBFilePath * );
  int (*link)( SBString *, SBFilePath * ); // Creates a hard link
  int (*unlink)( SBFilePath * );
  int (*getAttributes)( unsigned short, SBFilePath *, struct VfsGetAttribArgs *, struct FileAttributes ** );
  int (*setAttributes)( SBFilePath *, struct FileAttributes * );
  int (*fsctl)( int, char *, size_t, char **, size_t * );
};

struct VFS_Filesystem
{
  char name[VFS_NAME_MAXLEN];
  size_t nameLen;
  struct FSOps fsOps;
};

#endif
