#include "fatfs.h"
#include <string.h>
#include <stdlib.h>

/* This needs to get the FAT directly from the device rather than from a cache.
   (Mainly to prevent cache coherency problems) */

int writeFAT(struct FAT_Dev *fatDev, dword entry, unsigned cluster);
unsigned readFAT(struct FAT_Dev *fatDev, unsigned cluster);
unsigned getFreeCluster(struct FAT_Dev *fatDev);

extern int readSector( struct FAT_Dev *fatDev, unsigned sector, void *buffer );
extern int writeSector( struct FAT_Dev *fatDev, unsigned sector, void *buffer );

/* Sets the next cluster in a file cluster list */
/* Note that "unsigned cluster" serves as the index in the FAT. FAT[cluster] = 
nextCluster */

/*
  TODO: To be implemented...
  Assumes that a block size is at LEAST 3 bytes long. */

int writeFAT(struct FAT_Dev *fatDev, dword entry, unsigned cluster)
{
  unsigned sectorNum = cluster / FAT_SECTOR_SIZE;
  unsigned sectorOffset = cluster % FAT_SECTOR_SIZE;
  byte *buffer = malloc( FAT_SECTOR_SIZE );
  unsigned clusSize;
  byte bytes[4];

  if( buffer == NULL)
    return -1;

  if( cluster >= fatDev->cache.clusterCount )
    goto fat_error;

  if( fatDev->cache.fatType == FAT12 )
  {
    sectorNum = (3 * cluster / 2) / FAT_SECTOR_SIZE;
    sectorOffset = (3 * cluster / 2) % FAT_SECTOR_SIZE;
  }
  else if( fatDev->cache.fatType == FAT16 )
  {
    clusSize = 2;
    sectorNum = (unsigned)(cluster * clusSize) / FAT_SECTOR_SIZE;
    sectorOffset = (unsigned)(cluster * clusSize) % FAT_SECTOR_SIZE;
  }
  else
  {
    clusSize = 4;
    sectorNum = (unsigned)(cluster * clusSize) / FAT_SECTOR_SIZE;
    sectorOffset = (unsigned)(cluster * clusSize) % FAT_SECTOR_SIZE;
  }

  // read in either 2 or 4 bytes

  if( sectorOffset + (fatDev->cache.fatType == FAT12 ? 2 : clusSize) >= FAT_SECTOR_SIZE )
  {
    size_t off;

    if( readSector( fatDev, fatDev->bpb.fat12.resd_secs + sectorNum, buffer ) < 0 )
      goto fat_error;

    off = FAT_SECTOR_SIZE - sectorOffset;

    if( fatDev->cache.fatType == FAT12 )
    {
      if( (cluster % 2) == 0 )
        buffer[FAT_SECTOR_SIZE-1] = entry & 0xFF;
      else
        buffer[FAT_SECTOR_SIZE-1] = (buffer[FAT_SECTOR_SIZE-1] & 0x0F) | ((entry & 0x0F) << 4);
    }
    else
      buffer[FAT_SECTOR_SIZE-off] = entry & ((1 << (8 * off))-1);

    /* Write the first portion. */

    if( writeSector( fatDev, fatDev->bpb.fat12.resd_secs + sectorNum, buffer ) < 0 )
      goto fat_error;

    if( readSector( fatDev, fatDev->bpb.fat12.resd_secs + sectorNum + 1, buffer ) < 0 )
      goto fat_error;

    if( fatDev->cache.fatType == FAT12 )
    {
      if( (cluster % 2) == 0 )
        buffer[0] = (entry >> 8) | (buffer[0] & 0xF0);
      else
        buffer[0] = entry >> 4;
    }
    else
      buffer[0] = entry >> (8*((fatDev->cache.fatType == FAT12 ? 2 : (unsigned)clusSize)-off));

    /* Write the second portion. */

    if( writeSector( fatDev, fatDev->bpb.fat12.resd_secs + sectorNum + 1, buffer ) < 0 )
      goto fat_error;
  }
  else
  {
    if( readSector( fatDev, fatDev->bpb.fat12.resd_secs + sectorNum, buffer ) < 0 )
      goto fat_error;

    if(fatDev->cache.fatType == FAT12)
    {
      if((cluster % 2) == 0)
      {
        buffer[sectorOffset] = entry & 0xFF;
        buffer[sectorOffset+1] = ((entry >> 8) & 0x0F) | (buffer[sectorOffset+1] & 0xF0);
      }
      else
      {
        buffer[sectorOffset] = (bytes[0] & 0x0F) | ((entry & 0xF) << 4);
        buffer[sectorOffset+1] = (entry & 0xFF0) >> 4;
      }
    }
    else
      memcpy(&buffer[sectorOffset], &entry, clusSize);

    if( writeSector( fatDev, fatDev->bpb.fat12.resd_secs + sectorNum, buffer ) < 0 )
      goto fat_error;
  }

  free(buffer);
  return 0;

fat_error:
  free(buffer);
  return -1;
}

/* Retrieves the next cluster in a file cluster list.
   Note that "unsigned cluster" serves as the index in the FAT. FAT[cluster] = nextCluster
   Returns 1 on error */

unsigned readFAT(struct FAT_Dev *fatDev, unsigned cluster)
{
  unsigned sectorNum = cluster / FAT_SECTOR_SIZE;
  unsigned sectorOffset = cluster % FAT_SECTOR_SIZE;
  byte *buffer = malloc( FAT_SECTOR_SIZE );
  unsigned clusSize;
  byte entry[4];

  if( buffer == NULL)
    return 1;

  if( cluster >= fatDev->cache.clusterCount )
    goto fat_error;

  if( fatDev->cache.fatType == FAT12 )
  {
    sectorNum = (3 * cluster / 2) / FAT_SECTOR_SIZE;
    sectorOffset = (3 * cluster / 2) % FAT_SECTOR_SIZE;
  }
  else if( fatDev->cache.fatType == FAT16 )
  {
    clusSize = 2;
    sectorNum = (unsigned)(cluster * clusSize) / FAT_SECTOR_SIZE;
    sectorOffset = (unsigned)(cluster * clusSize) % FAT_SECTOR_SIZE;
  }
  else
  {
    clusSize = 4;
    sectorNum = (unsigned)(cluster * clusSize) / FAT_SECTOR_SIZE;
    sectorOffset = (unsigned)(cluster * clusSize) % FAT_SECTOR_SIZE;
  }

  // read in either 2 or 4 bytes

  if( sectorOffset + (fatDev->cache.fatType == FAT12 ? 2 : clusSize) >= FAT_SECTOR_SIZE )
  {
    size_t off;

    if( readSector( fatDev, fatDev->bpb.fat12.resd_secs+sectorNum, buffer ) < 0 )
      goto fat_error;

    off = FAT_SECTOR_SIZE - sectorOffset;
    memcpy(entry, &buffer[sectorOffset], off);

    if( readSector( fatDev, fatDev->bpb.fat12.resd_secs+sectorNum+1, buffer ) < 0 )
      goto fat_error;

    memcpy(&entry[off], buffer, (fatDev->cache.fatType == FAT12 ? 2 : clusSize) - off);
  }
  else
  {
    if( readSector( fatDev, fatDev->bpb.fat12.resd_secs, buffer ) < 0 )
      goto fat_error;

    memcpy(entry, &buffer[sectorOffset], (fatDev->cache.fatType == FAT12 ? 2 : clusSize));
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
  return 1;
}

/* Returns a free cluster(if any) in the FAT.
   Returns 1 on failure. */

unsigned getFreeCluster(struct FAT_Dev *fatDev)
{
  byte *buffer;
  unsigned entry;

  if( fatDev == NULL )
    return 1;

  buffer = malloc( fatDev->cache.fatSize * FAT_SECTOR_SIZE );

  if( buffer == NULL )
    return 1;

  for( unsigned i=0; i < fatDev->cache.fatSize; i++ )
  {
    if( readSector( fatDev, fatDev->bpb.fat12.resd_secs+i, buffer + FAT_SECTOR_SIZE * i ) < 0 )
      return 1;
  }

  for( unsigned i=2; i < fatDev->cache.clusterCount; i++ )
  {
    switch(fatDev->cache.fatType)
    {
      case FAT12:
        if( (i % 2) == 1 )
         entry = (unsigned)(((buffer[(3 * i) / 2] & 0xF0) >> 4) | (buffer[1 + (3*i)/2] << 4));
        else
         entry = (unsigned)(buffer[(3*i)/2] | ((buffer[1 + (3*i)/2] & 0x0F) << 8));
         break;
      case FAT16:
        entry =  (unsigned)*((word *)&buffer[sizeof(word)*i]);
        break;
      case FAT32:
        entry = (unsigned)*((dword *)&buffer[sizeof(dword)*i]);
        break;
      default:
        goto fat_error;
    }

    if( entry == 0 )
    {
      free( buffer );
      return i;
    }
  }
fat_error:
  free(buffer);
  return 1;
}
