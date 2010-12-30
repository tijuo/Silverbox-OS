#ifndef FILE_H
#define FILE_H

#define PIPE_SIZE	4096

/*
struct char_dev {
  short drv_type;
  short dev_num;
  void *buffer;
  long flags;
}  

struct block_dev {
  short drv_type;
  short dev_num;
  void *buffer;
  int block size;
  long flags;
};
*/

#define ATR_FILE		0x00
#define ATR_DIR			0x01
#define ATR_HIDDEN		0x02
#define ATR_READ		0x04
#define ATR_NOREAD		0x00
#define ATR_WRITE		0x08
#define ATR_NOWRITE		0x00

#define WR_MODE                 0x01
#define RD_MODE                 0x02
#define RW_MODE			(WR_MODE | RD_MODE)

#define FL_OPEN                 0x01
#define FL_EOF			0x02

#define TYPE_CHAR		0x01
#define TYPE_BLOCK		0x02
#define TYPE_PIPE		0x04

#define SEEK_BEG        	0
#define SEEK_CURR        	1
#define SEEK_LAST        	2

#define MAX_FHANDLES		16

struct file
{
  int dev_num;
  void *buffer;
  int size;
  struct file_ops *fops;
  struct vfile *vfile;
  int pos;
  int mode;
  int flags;
  int lock;
  int type;
};

struct file_ops {
  int (*open)(struct file *, int);
  int (*close)(struct file *);
  int (*seek)(struct file *, int, int);
  int (*read)(struct file *, void *, unsigned);
  int (*write)(struct file *, void *, unsigned);
  int (*ioctl)(int, int, long);
};
#endif /* FILE */
