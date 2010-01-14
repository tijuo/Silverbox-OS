/* The VFS is in charge of keeping a record of all registered filesystems. */

// 1. Filesystem list (filesystem, server)
// 2. Mount Table (filesystem, virtual path, device, flags)



/* The VFS will appear to act as a single, global filesystem while it translates VFS requests into filesystem specific requests (for FAT, ext2, ntfs, etc.).*/

/*
  statuses:
    Path not found 
    Bad parameter(s)
    Operation Failed

Uniform interface with other filesystems. These are, essentially, wrapper
 functions that will simply forward requests to the respective filesystem.

  enum FsRequest { CREATE_DIR, LIST_DIR, CREATE_FILE, READ_FILE, WRITE_FILE,
                   REMOVE, LINK, UNLINK, GET_ATTRIB, SET_ATTRIB, FSCTL,
                   MOUNT, UNMOUNT };

  int createDir( char *name, size_t name_len, char *path, size_t path_len );
  int listDir( char *path, size_t path_len );
  int createFile( char *name, size_t name_len, char *path, size_t path_len );
  int readFile( char *path, size_t path_len );
  int writeFile( char *path, size_t path_len, char *buffer, size_t buflen );
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

  struct FsReqMsg
  {
    int req;
    size_t pathLen;

    union {
      size_t nameLen;
      size_t dataLen;
    };

    char data[];
  };
*/
