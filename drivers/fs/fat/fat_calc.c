#include <fatfs.h>
#include <string.h>

/* Hmm... I wonder what parse_dirname() and parse_name() do... */

/* '*name' is not NULL terminated. */
/*
char *parse_dirname(char *name)
{
  int i;

  char *newname;

  if(newname == NULL)
    return NULL;

  if(name[0] == '.')
    return NULL;

  if(strlen(name) > 11)
    return NULL;

  newname = (char *)malloc(12);

  memset(newname, 0, 11);
//  strcpy(newname, name);

  for(i=0; i < 11; i++) {
    switch(name[i]) {
      case 0x22:
      case 0x2A:
      case 0x2B:
      case 0x2C:
      case 0x2E:
      case 0x2F:
      case 0x3A:
      case 0x3B:
      case 0x3C:
      case 0x3D:
      case 0x3E:
      case 0x3F:
      case 0x5B:
      case 0x5C:
      case 0x5D:
      case 0x7C:
        free(newname);
        return NULL;
        break;
      case '\0':
        memset(&newname[i], ' ', 11 - i);
        i = 11;
        break;
      default:
        newname[i] = toupper(name[i]);
        break;
    }
  }
  newname[12] = '\0';
  return newname;
}

char *parse_filename(char *name)
{
  int i, j, cont = 8;
  char *newname;

  if(strlen(name) > 11)
    return NULL;

  if(name[0] == '.')
    return NULL;

  newname = (char *)malloc(12);

  for(i=0; i < 8; i++) {
    switch(name[i]) {
      case '"':
      case '*':
      case '+':
      case ',':
      case '/':
      case ':':
      case ';':
      case '<':
      case '=':
      case '>':
      case '?':
      case '[':
      case '\\':
      case ']':
      case '|':
        free(newname);
        return NULL;
        break;
      case '.':
        if(i < 8)
          memset(&newname[i], ' ', 8 - i);
	    cont = i + 1;
	    i = 8;
        break;
      case 0:
        memset(&newname[i], ' ', 11 - i);
        i = 11;
	    cont = 12;
        break;
      default:
        newname[i] = toupper(name[i]);
        break;
    }
  }

  for(i=cont, j = 8; i < 12; i++, j++) {
    switch(name[i]) {
      case '"':
      case '*':
      case '+':
      case ',':
      case '/':
      case ':':
      case ';':
      case '<':
      case '=':
      case '>':
      case '?':
      case '[':
      case '\\':
      case ']':
      case '|':
      case '.':
        free(newname);
        return NULL;
        break;
      case 0:
        memset(&newname[j], ' ', 11 - j);
        i = 12;
        break;
      default:
        newname[j] = toupper(name[i]);
        break;
    }
  }

  strncpy(name, newname, 11);
  newname[12] = '\0';
  return newname;
}
*/

/* Calculates the number of sectors allocated for the root directory */

int calcRootDirSecs(union FAT_BPB *bpb)
{
  int bytes_per_sec, rootdirs, type;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
  {
    rootdirs = bpb->fat12.root_ents;
    bytes_per_sec = bpb->fat12.bytes_per_sec;
  }
  else if(type == FAT16)
  {
    rootdirs = bpb->fat16.root_ents;
    bytes_per_sec = bpb->fat16.bytes_per_sec;
  }

  return (rootdirs * 32) / bytes_per_sec;
}

/* Returns the first data sector after the BPB, FAT, and
   Root Dirs */

int firstDataSec(union FAT_BPB *bpb)
{
  int fatsize, resd, copies, type;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
  {
    fatsize = bpb->fat12.secs_per_fat;
    resd = bpb->fat12.resd_secs;
    copies = bpb->fat12.fat_copies;
  }
  else if(type == FAT16)
  {
    fatsize = bpb->fat16.secs_per_fat;
    resd = bpb->fat16.resd_secs;
    copies = bpb->fat16.fat_copies;
  }

  if(fatsize == 0)
    fatsize = bpb->fat32.secs_per_fat;

  return resd + (copies * fatsize) + calcRootDirSecs(bpb);
}

/* Returns the first sector of the cluster provided.
     (Data Clusters only!) */

int calcFirstSecClus(int clusnum, union FAT_BPB *bpb)
{
  int size, type;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
    size = bpb->fat12.secs_per_clus;
  else if(type == FAT16)
    size = bpb->fat16.secs_per_clus;

  if(size == 0)
    size = bpb->fat32.secs_per_clus;

  return ((clusnum - 2) * size) +  (firstDataSec(bpb) / size);
}

int calcDataSecs(union FAT_BPB *bpb)
{
  int fatsize, total, resd, copies, type;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
  {
    fatsize = bpb->fat12.secs_per_fat;
    total = bpb->fat12.total_secs;
    resd = bpb->fat12.resd_secs;
    copies = bpb->fat12.fat_copies;
  }
  else if(type == FAT16)
  {
    fatsize = bpb->fat16.secs_per_fat;
    total = bpb->fat16.total_secs;
    resd = bpb->fat16.resd_secs;
    copies = bpb->fat16.fat_copies;
  }

  if(fatsize == 0)
    fatsize = bpb->fat32.secs_per_fat;

  if(total == 0)
    total = bpb->fat32.secs_in_fs;

  return total - resd + (copies * fatsize) + calcRootDirSecs(bpb);
}

/* Calculate the amount of clusters that are present on the disk.
   (Data Clusters only!) */

int calcClusCount(union FAT_BPB *bpb)
{
  int secs_per_clus, type;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
    secs_per_clus = bpb->fat12.secs_per_clus;
  else if(type == FAT16)
    secs_per_clus = bpb->fat16.secs_per_clus;

  return calcDataSecs(bpb) / secs_per_clus;
}

enum FAT_Type determineFAT_Type(union FAT_BPB *bpb)
{
  unsigned count = bpb->fat12.total_secs;

  if(count < 4085)
    return FAT12;
  else if(count < 65525)
    return FAT16;
  else
    return FAT32;
}

/* Find the the size of one FAT in sectors.
   !!! Broken. !!! */

int calcFatSize(union FAT_BPB *bpb)
{
  int totalsecs, resdsecs, rootents, clussize, secsize;
  float result;
  enum FAT_Type type;
  float fat_len;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
  {
    secsize = bpb->fat12.bytes_per_sec;
    clussize = bpb->fat12.secs_per_clus;
    resdsecs = bpb->fat12.resd_secs;
    rootents = bpb->fat12.root_ents;
    totalsecs = bpb->fat12.total_secs;
    fat_len = 1.5;
  }
  else if(type == FAT16)
  {
    secsize = bpb->fat16.bytes_per_sec;
    clussize = bpb->fat16.secs_per_clus;
    resdsecs = bpb->fat16.resd_secs;
    rootents = bpb->fat16.root_ents;
    totalsecs = bpb->fat16.total_secs;
    fat_len = 2;
  }

  result = (fat_len * (totalsecs - resdsecs - ((rootents * 32) / secsize))) / \
	(clussize * secsize + 4);

  if(result != (int)result)
    result = (int)result + 1;

  return (int)result;
}

int calcClusterSize( union FAT_BPB *bpb )
{
  enum FAT_Type type;

  type = determineFAT_Type(bpb);

  if(type == FAT12)
    return bpb->fat12.secs_per_clus * bpb->fat12.bytes_per_sec;
  else if(type == FAT16)
    return bpb->fat16.secs_per_clus * bpb->fat16.bytes_per_sec;

  return -1;
}

enum FAT_ClusterType getClusterType( unsigned cluster, enum FAT_Type type )
{
  switch( type )
  {
    case FAT12:
      if( cluster == 0 )
        return FREE_CLUSTER;
      else if( cluster >= 0xFF8 && cluster <= 0xFFF )
        return END_CLUSTER;
      else if( cluster >= 2 && cluster <= 0xFEF )
        return USED_CLUSTER;
      else if( cluster == 0xFF7 )
        return BAD_CLUSTER;
      else if( cluster == 1 || (cluster >= 0xFF0 && cluster <= 0xFF6) )
        return RESD_CLUSTER;
      break;
    case FAT16:
      if( cluster == 0 )
        return FREE_CLUSTER;
      else if( cluster >= 0xFFF8 && cluster <= 0xFFFF )
        return END_CLUSTER;
      else if( cluster >= 2 && cluster <= 0xFFEF )
        return USED_CLUSTER;
      else if( cluster == 0xFFF7 )
        return BAD_CLUSTER;
      else if( cluster == 1 || (cluster >= 0xFFF0 && cluster <= 0xFFF6) )
        return RESD_CLUSTER;
      break;
    case FAT32:
      cluster &= 0xFFFFFFF;

      if( cluster == 0 )
        return FREE_CLUSTER;
      else if( cluster >= 0xFFFFFF8 && cluster <= 0xFFFFFFF )
        return END_CLUSTER;
      else if( cluster >= 2 && cluster <= 0xFFFFFEF )
        return USED_CLUSTER;
      else if( cluster == 0xFFFFFF7 )
        return BAD_CLUSTER;
      else if( cluster == 1 || (cluster >= 0xFFFFFF0 && cluster <= 0xFFFFFF6) )
        return RESD_CLUSTER;
    default:
      return INVALID_CLUSTER;
  }
  return INVALID_CLUSTER;
}
