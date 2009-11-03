#ifndef VIRTUAL_FS
#define VIRTUAL_FS

#include <types.h>
#include <os/file.h>
//#include <time.h>

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

enum FsRequest { CREATE_DIR, LIST_DIR, CREATE_FILE, READ_FILE, WRITE_FILE,
                 REMOVE, LINK, UNLINK, GET_ATTRIB, SET_ATTRIB, FSCTL /*,
                 MOUNT, UNMOUNT*/ };

struct FileAttributes
{
  int flags;
  long long timestamp; // number of microseconds since Jan. 1, 1950 CE
  long long size;
  unsigned char nameLen;
  char name[255];
};

struct FsReqMsg
{
  int req;
  size_t pathLen;
  unsigned short devNum;

  union {
    size_t nameLen;
    size_t dataLen;
  };

  size_t offset;
};

struct FsReplyMsg
{
  int fail;
  size_t bufferLen;
};





/* Error codes */

#define EGEN      1 /* Generic error */
#define EOTH      2 /* Other error */
#define ENDIR     3 /* Not a dir error */
#define EDEV      4 /* Device failure error */
#define EBUG      5 /* Possible bug error */

/* convert_path() error codes */

#define ECP_NOTDIR          1
#define ECP_NOTEXIST        2

#define FL_DIRMOUNT         0x80
#define FL_DIRDIRTY         0x40
#define FL_DIRROOT          0x20
#define FL_DIRRONLY         0x10
#define FL_DIRCACHE         0x08

#define FL_FILEOF           0x80
#define FL_FILDIRTY         0x40
#define FL_FILRONLY         0x20
#define FL_FILDLINK         0x10
#define FL_FILFLINK         0x08

#define ATR_DIRECTORY       0x01
#define ATR_FILE            0x00

#define ATR_DEVICE          0x02


struct FS_ops {
//  int init(struct FS_header *header, int dev, int flags);
//int flush();  aka sync() only used in RW FS
  int (*cleanup)(void);
};

//int fat_read_file(struct file *, void *, unsigned);
/*
struct file_ops fat_fops = {
  read: fat_read_file;
};
*/
struct FS_header {
//  generic FS info...
  int read_only;
  int blk_size;
  int blk_count;
  int flags;
  int device;

  struct file_ops *f_ops;
  struct dir_ops  *d_ops;

  struct vdir *mnt_point;
  struct vdir *root;

  struct filesystem *filesys;

//  struct FS_ops *fs_ops;
//  FS specific info...
  void *fs_info;
  struct FS_header *next;
};

struct filesystem {
  char *name;
  int (*init)(struct vdir *, int, int);
  int other;
  struct filesystem *next;
};

struct vdir {
  char *name;
  int  size;  // Should always equal 0
  int  attrib;
  int  flags;
  int  used; // Indicates how many processes are using this

  int blk_start;
  int blk_count;
  int blk_size;
/*
  struct tm date_created;
  struct tm date_modified;
  struct tm date_accessed;
*/
  struct vdir *root;
  struct vdir *parent;
  struct FS_header *fs;

  struct dir_ops *d_ops;
  struct vdir *subdirs;
  struct vfile *file_list;

  struct vdir *prev;
  struct vdir *next;
  void *fs_info;
};

struct vfile {
  char *name;
  int  size;
  int  attrib;
  int  flags;
  int  used; // Indicates the amount of processes using this
  int  type;
  int  dev_num;

  int blk_start;
  int blk_count;
  int blk_size;
/*
  struct tm date_created;
  struct tm date_modified;
  struct tm date_accessed;
*/
  struct vdir *root;
  struct vdir *parent;
  struct FS_header *fs;

  struct file_ops *f_ops;
  union {
    struct vfile *file;
    struct vdir  *dir;
  } link;

  struct vfile *prev;
  struct vfile *next;
  void *fs_info;
};

struct dir_ops {
  int (*list)(struct vdir *, struct vfile **, struct vdir **);
  int (*locate)(char *, struct vdir *, struct vfile **, struct vdir **);
// int rename(char *, char *, vdir *);
};

struct FS_header *fsheader_list;
struct filesystem *fs_list;
struct vdir *vfs_root, *vfs_cwdir, *vfs_device, *vfs_mount;

struct vdir *vndir(char *name, struct vdir *parent);
struct vfile *vnfile(char *name, struct vdir *parent);
void vldir(struct vdir *wd);
struct file *vopen(char *name, int mode);
struct vdir *vcd(char *name, struct vdir *parent/*, int *error*/);
struct vdir *vcwd(void);

struct FS_header *GetFS(int dev, char *name);
struct filesystem *FindFS(char *name);

struct vdir *convert_path(char *path, int *error/* struct vdir *filename*/);
int vfs_cd(char *path);
int vdir_cd(struct vdir *dir);
int vfs_cwd(char *path, int buflen);

int sys_cd(char *path);
int sys_ldir(char *path);
int sys_cwd(char *path, int buflen);

#endif
