#ifndef FS_H
#define FS_H

#include <types.h>
/*
#define CREATE_FILE		0xE0
#define CREATE_DIR		0xE1
#define CREATE_LINK		0xE2
#define GET_LISTING		0xE3
#define MODIFY_ATTRIB		0xE4
#define DELETE_FILE		0xE5
#define DELETE_DIR		0xE6
#define DELETE_LINK		0xE7
#define OPEN_FILE		0xE8
#define READ_FILE		0xE9
#define WRITE_FILE		0xEA
#define CLOSE_FILE		0xEB
#define FSCTL			0xEC

#define FS_RDPERM		0x01
#define FS_WRPERM		0x02
#define FS_EXPERM		0x04
#define FS_HIDDEN		0x08
#define FS_DIR			0x10

typedef long date_t;
/ *
struct FS_Record
{
  char name[13];
  char flags;
  mid_t mbox;
};
* /
struct DirEntry
{
  short entryLen;
  short  flags;
  size_t fileLen;
  date_t timeStamp;
  char fileName[];
} __PACKED__;
*/
#endif
