#ifndef FAT_FS
#define FAT_FS

#include <types.h>
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
    uint16_t *fat12;  // 12-bits
    uint16_t *fat16;
    uint32_t *fat32;
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
  uint8_t  jmp_bootstrap[3];
  uint8_t  name[8];
  uint16_t  uint8_ts_per_sec;
  uint8_t  secs_per_clus;
  uint16_t  resd_secs;
  uint8_t  fat_copies;
  uint16_t  root_ents;
  uint16_t  total_secs;
  uint8_t  type;
  uint16_t  secs_per_fat16; /* Should be zero */
  uint16_t  secs_per_track;
  uint16_t  heads;
  uint32_t hidden_secs;
  uint32_t secs_in_fs;
  uint32_t secs_per_fat;
  uint16_t  mirror_flags;
  uint16_t  fs_version;
  uint32_t root_first_clus;
  uint16_t  backup_bootsector;
  uint8_t  _resd[13];
  uint8_t  drive_num;
  uint8_t  _resd2;
  uint8_t  ext_signature;
  uint32_t serial_num;
  uint8_t  label[11];
  uint8_t  fs_type[8];
  uint8_t  bootstrap[410];
  uint16_t  signature;
} PACKED;

struct FAT12_BPB
{
  uint8_t  jmp_bootstrap[3];
  uint8_t  name[8];
  uint16_t  uint8_ts_per_sec;
  uint8_t  secs_per_clus;
  uint16_t  resd_secs;
  uint8_t  fat_copies;
  uint16_t  root_ents;
  uint16_t  total_secs;
  uint8_t  type;
  uint16_t  secs_per_fat;
  uint16_t  secs_per_track;
  uint16_t  heads;
  uint32_t hidden_secs;
  uint32_t large_secs;
  uint8_t  drive_num;
  uint8_t  _resd;
  uint8_t  ext_signature;
  uint32_t serial_num;
  uint8_t  label[11];
  uint8_t  fs_type[8];
  uint8_t  bootstrap[448];
  uint16_t  signature;
} PACKED;

struct FAT16_BPB {
  uint8_t  jmp_bootstrap[3];
  uint8_t  name[8];
  uint16_t  uint8_ts_per_sec;
  uint8_t  secs_per_clus;
  uint16_t  resd_secs;
  uint8_t  fat_copies;
  uint16_t  root_ents;
  uint16_t  total_secs;
  uint8_t  type;
  uint16_t  secs_per_fat;
  uint16_t  secs_per_track;
  uint16_t  heads;
  uint32_t hidden_secs;
  uint32_t large_secs;
  uint8_t  drive_num;
  uint8_t  _resd;
  uint8_t  ext_signature;
  uint32_t serial_num;
  uint8_t  label[11];
  uint8_t  fs_type[8];
  uint8_t  bootstrap[426];
  uint16_t  signature;
} PACKED;

struct FAT_DirEntry
{
  uint8_t  filename[11];
  uint8_t  attrib;
  uint8_t  _resd[10];
  uint16_t  time;
  uint16_t  date;
  uint16_t  start_clus;
  uint32_t file_size;
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
