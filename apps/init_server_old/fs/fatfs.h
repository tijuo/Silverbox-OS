#ifndef FAT_FS
#define FAT_FS

#include "../../../include/type.h"
#include <os/device.h>

/* Only the Data Area is divided into clusters!
   The other areas are made up of sectors */

#define CLUSTERTOBLK(clus, fat_dev)   (clus * fat_dev->info.fat12.secs_per_clus * fat_dev->info.fat12.fat_copies + (fat_dev->cache.fat_size + 1 \
                                      + fat_dev->cache.root_dir_secs))
#define BLKTOCLUSTER(blk, info, size)    ((blk / size) - (FirstDataSec(&info) / (size * size)) + 2)

#define FAT_RDONLY		0x01
#define FAT_HIDDEN		0x02
#define FAT_SYSTEM		0x04
#define FAT_VLABEL		0x08
#define FAT_SUBDIR		0x10
#define FAT_ARCHIVE		0x20
#define FAT_DEVICE		0x40

#define FAT_LONG_FNAME		0x0F

#define FAT_SECTOR_SIZE		512
#define FAT_FREE_CLUSTER	0

enum FAT_Type { FAT12, FAT16, FAT32 };

enum FAT_ClusterType { INVALID_CLUSTER = -1, FREE_CLUSTER, USED_CLUSTER,
                       RESD_CLUSTER, BAD_CLUSTER, END_CLUSTER };

struct fatCache
{
/*  union
  {
    word *fat12;  // 12-bits
    word *fat16;
    dword *fat32;
  } table; */

  unsigned rootDirSecs;
  unsigned firstDataSec;
  unsigned numDataSecs;
  unsigned clusterCount;
  enum FAT_Type fatType;
  unsigned fatSize;
};

struct FAT32_BPB
{
  byte  jmp_bootstrap[3];
  byte  name[8];
  word  bytes_per_sec;
  byte  secs_per_clus;
  word  resd_secs;
  byte  fat_copies;
  word  root_ents;
  word  total_secs;
  byte  type;
  word  secs_per_fat16; /* Should be zero */
  word  secs_per_track;
  word  heads;
  dword hidden_secs;
  dword secs_in_fs;
  dword secs_per_fat;
  word  mirror_flags;
  word  fs_version;
  dword root_first_clus;
  word  backup_bootsector;
  byte  _resd[13];
  byte  drive_num;
  byte  _resd2;
  byte  ext_signature;
  dword serial_num;
  byte  label[11];
  byte  fs_type[8];
  byte  bootstrap[410];
  word  signature;
} PACKED;

struct FAT12_BPB
{
  byte  jmp_bootstrap[3];
  byte  name[8];
  word  bytes_per_sec;
  byte  secs_per_clus;
  word  resd_secs;
  byte  fat_copies;
  word  root_ents;
  word  total_secs;
  byte  type;
  word  secs_per_fat;
  word  secs_per_track;
  word  heads;
  dword hidden_secs;
  dword large_secs;
  byte  drive_num;
  byte  _resd;
  byte  ext_signature;
  dword serial_num;
  byte  label[11];
  byte  fs_type[8];
  byte  bootstrap[448];
  word  signature;
} PACKED;

struct FAT16_BPB {
  byte  jmp_bootstrap[3];
  byte  name[8];
  word  bytes_per_sec;
  byte  secs_per_clus;
  word  resd_secs;
  byte  fat_copies;
  word  root_ents;
  word  total_secs;
  byte  type;
  word  secs_per_fat;
  word  secs_per_track;
  word  heads;
  dword hidden_secs;
  dword large_secs;
  byte  drive_num;
  byte  _resd;
  byte  ext_signature;
  dword serial_num;
  byte  label[11];
  byte  fs_type[8];
  byte  bootstrap[426];
  word  signature;
} PACKED;

struct FAT_DirEntry
{
  byte  filename[11];
  byte  attrib;
  byte  _resd[10];
  word  time;
  word  date;
  word  start_clus;
  dword file_size;
} PACKED;

union FAT_BPB
{
  struct FAT12_BPB fat12;
  struct FAT16_BPB fat16;
  struct FAT32_BPB fat32;
};

struct FAT_Dev
{
  unsigned short deviceNum;
  struct Device device;
  union FAT_BPB bpb;
  enum FAT_Type type;
  struct fatCache cache;
};

#endif /* FAT_FS */
