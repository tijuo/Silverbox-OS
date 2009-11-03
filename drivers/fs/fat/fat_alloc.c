#include <fatfs.h>
#include <string.h>
#include <stdlib.h>

/* This needs to get the FAT directly from the device rather than from a cache.
   (Mainly to prevent cache coherency problems) */

int writeFAT(struct FAT_Dev *fatDev, dword entry, unsigned cluster);
unsigned readFAT(struct FAT_Dev *fatDev, unsigned cluster);
int getFreeCluster(struct FAT_Dev *fatDev);

extern int readSector( struct FAT_Dev *fatDev, unsigned sector, void *buffer );

/* Sets the next cluster in a file cluster list */
/* Note that "unsigned cluster" serves as the index in the FAT. FAT[cluster] = 
nextCluster */

/*
  XXX: To be implemented...

int writeFAT(struct FAT_Dev *fatDev, dword entry, unsigned cluster)
{
  byte *fat12Addr;

  switch(fatDev->cache.fatType)
  {
    case FAT12:
      fat12Addr = (byte *)((unsigned)fatDev->cache.table.fat12 + 3 * (cluster / 2 * 3));

      if((cluster % 2) == 0)
      {
        fat12Addr[0] = 	entry & 0xFF;
        fat12Addr[1] = ((entry & 0xF00) >> 4) | (fat12Addr[1] & 0xF0);
      }
      else
      {
        fat12Addr[1] = (fat12Addr[0] & 0xF0) | (entry & 0xF);
        fat12Addr[2] = (entry & 0xFF0) >> 4;
      }
      break;
    case FAT16:
      fatDev->cache.table.fat16[cluster] = entry & 0xFFFF;
      break;
    case FAT32:
      fatDev->cache.table.fat32[cluster] = entry;
    default:
      return -1;
  }
  return 0;
}
*/

/* Retrieves the next cluster in a file cluster list. */
/* Note that "unsigned cluster" serves as the index in the FAT. FAT[cluster] = nextCluster */

unsigned readFAT(struct FAT_Dev *fatDev, unsigned cluster)
{
  unsigned sectorNum = cluster / FAT_SECTOR_SIZE;
  unsigned sectorOffset = cluster % FAT_SECTOR_SIZE;
  byte *buffer = malloc( FAT_SECTOR_SIZE );
  float clusSize;
  byte entry[4];

  if( buffer == NULL)
    return 0;

  if( cluster >= fatDev->cache.clusterCount )
    goto fat_error;

  if( fatDev->cache.fatType == FAT12 )
    clusSize = 1.5f;
  else if( fatDev->cache.fatType == FAT16 )
    clusSize = 2;
  else
    clusSize = 4;

  sectorNum = (unsigned)(cluster * clusSize) / FAT_SECTOR_SIZE;
  sectorOffset = (unsigned)(cluster * clusSize) % FAT_SECTOR_SIZE;

  // read in either 2 or 4 bytes

  if( sectorOffset + (unsigned)clusSize >= FAT_SECTOR_SIZE )
  {
    size_t off;

    if( readSector( fatDev, fatDev->bpb.fat12.resd_secs, buffer ) < 0 )
      goto fat_error;

    off = FAT_SECTOR_SIZE - sectorOffset;
    memcpy(entry, &buffer[sectorOffset], off);

    if( readSector( fatDev, fatDev->bpb.fat12.resd_secs, buffer ) < 0 )
      goto fat_error;

    memcpy(&entry[off], buffer, (clusSize == 1.5f ? 2 : clusSize) - off);
  }
  else
  {
    if( readSector( fatDev, fatDev->bpb.fat12.resd_secs, buffer ) < 0 )
      goto fat_error;

    memcpy(entry, &buffer[sectorOffset], (clusSize == 1.5f ? 2 : clusSize));
  }

  switch(fatDev->cache.fatType)
  {
    case FAT12:
     if( (cluster % 2) == 1 )
        return (unsigned)(((entry[0] >> 4) & 0x0F) | (entry[1] << 4));
     else
        return (unsigned)(entry[0] | ((entry[1] & 0x0F) << 8));
    case FAT16:
      return (unsigned)*((word *)entry);
    case FAT32:
      return (unsigned)*((dword *)entry);
    default:
      goto fat_error;
  }

fat_error:
  free(buffer);
  return 0;
}

/* Returns a free cluster(if any) in the FAT.
   Returns -1 on failure. */

/*

  To be implemented
int getFreeCluster(struct FAT_Dev *fatDev)
{
  byte *fat12Addr;

  switch(fatDev->cache.fatType)
  {
    case FAT12:
    {
      word value;

      for( unsigned i=2; i < fatDev->cache.clusterCount; i++ )
      {
        fat12Addr = (byte *)((unsigned)fatDev->cache.table.fat12 + 
                     3 * (i / 2));

        if( i % 2 )
          value = (fat12Addr[1] >> 4) | (fat12Addr[2] << 4);
        else
          value = fat12Addr[0] | ((fat12Addr[1] & 0x0F) << 4);

        if( getClusterType(value, FAT12) == FREE_CLUSTER )
          return i;
      }
      break;
    }
    case FAT16:
      for( unsigned i=2; i < fatDev->cache.clusterCount; i++ )
      {
        if( getClusterType(fatDev->cache.table.fat16, FAT16) == FREE_CLUSTER )
          return i;
      }
      break;
    case FAT32:
       for( unsigned i=2; i < fatDev->cache.clusterCount; i++ )
       {
         if( getClusterType(fatDev->cache.table.fat32, FAT32) == FREE_CLUSTER )
           return i;
       }
    default:
      return -1;
  }
  return -1;
}
*/