#include <os/vfs.h>
#include "../name.h"
#include <os/os_types.h>
#include <string.h>
#include <stdlib.h>

extern int fatReadFile( SBFilePath *path, unsigned int offset, unsigned short devNum,
                 char *fileBuffer, size_t length );
extern int fatGetDirList( SBFilePath *path, unsigned short devNum, struct FileAttributes *attrib,
                   size_t maxEntries );
extern int fatGetAttributes( SBFilePath *path, unsigned short devNum, struct FileAttributes *attrib );
extern int fatCreateFile( SBFilePath *path, const char *name, unsigned short devNum );
extern int fatCreateDir( SBFilePath *path, const char *name, unsigned short devNum );

int registerFAT(void);
static int fatList( unsigned short devNum, SBFilePath *path, 
  struct VfsListArgs *args, struct FileAttributes **attrib );
static int fatGetAttr( unsigned short devNum, SBFilePath *path, 
  struct VfsGetAttribArgs *args, struct FileAttributes **attrib );
static int fatRead( unsigned short devNum, SBFilePath *path, 
  struct VfsReadArgs *args, char **buffer );

static int fatList( unsigned short devNum, SBFilePath *path, 
  struct VfsListArgs *args, struct FileAttributes **attrib )
{
  int ret;
  *attrib = malloc(sizeof(struct FileAttributes) * args->maxEntries);

  if( !attrib )
    return -1;

  ret = fatGetDirList( path, devNum, *attrib, args->maxEntries );

  if( ret < 0 )
    free( *attrib );

  return ret;
}

static int fatGetAttr( unsigned short devNum, SBFilePath *path, 
  struct VfsGetAttribArgs *args, struct FileAttributes **attrib )
{
  int ret;
  *attrib = malloc(sizeof(struct FileAttributes));

  if( !attrib )
    return -1;

  ret = fatGetAttributes( path, devNum, *attrib );

  if( ret < 0 )
    free( *attrib );

  return ret;
}

static int fatRead( unsigned short devNum, SBFilePath *path, 
  struct VfsReadArgs *args, char **buffer )
{
  int ret;
  *buffer = malloc(args->length);

  if( !buffer )
    return -1;

  ret = fatReadFile( path, args->offset, devNum, *buffer, args->length );

  if( ret < 0 )
    free( *buffer );

  return ret;
}

int registerFAT(void)
{
  struct VFS_Filesystem *fs = calloc(1, sizeof (struct VFS_Filesystem));

  if( !fs )
    return -1;

  fs->fsOps.list = fatList;
  fs->fsOps.read = fatRead;
  fs->fsOps.getAttributes = fatGetAttr;

  fs->nameLen = 3;

  memcpy(fs->name, "fat", fs->nameLen);

  return _registerName(fs->name, fs->nameLen, FS, fs);
}
