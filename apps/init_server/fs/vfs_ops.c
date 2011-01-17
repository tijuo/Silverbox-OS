#include <os/vfs.h>
#include <os/os_types.h>
#include <string.h>

/**
  Takes in a path and returns a path relative to the mount point,
  its associated mount point, and the FS operations for the
  mounted device
*/

extern int sbFilePathCreate(SBFilePath *path);
extern int lookupMountEntry(const SBString *path, struct MountEntry **entry,
    SBFilePath *relPath);

int getFsOps(const char *path_str, SBFilePath *relPath, struct FSOps **ops, struct MountEntry **entry);


int getFsOps(const char *path_str, SBFilePath *relPath, struct FSOps **ops, struct MountEntry **entry)
{
  SBString path;

  sbFilePathCreate(relPath);
  sbStringCreate(&path, path_str);

  if( lookupMountEntry(&path, entry, relPath) != 0 )
    return -1;

  *ops = &(*entry)->fs->fsOps;

  sbStringDelete(&path);

  return 0;
}

int _readFile(const char *path, int offset, char *buffer, size_t bytes)
{
  SBFilePath relPath;
  struct FSOps *ops=NULL;
  struct MountEntry *entry = NULL;
  struct VfsReadArgs args;
  char *newBuffer = NULL;
  int num_read=0;

  if( getFsOps(path, &relPath, &ops, &entry) != 0 )
    return -1;

  args.offset = offset;
  args.length = bytes;

  num_read = ops->read(entry->device, &relPath, &args, &newBuffer);

  if( num_read >= 0 )
  {
    memcpy(buffer, newBuffer, args.length);
    free(newBuffer);
  }

  return num_read;
}

/*
    case CREATE_DIR:
      ret = ops->createDir(&name, &fPath);
      break;
    case LIST:
    {
      struct VfsListArgs *args = (struct VfsListArgs *)argStart;
      ret = ops->list(entry->device, &fPath, args, (struct FileAttributes **)outBuffer);
       *outBytes = (ret < 0 ? 0 : ret * sizeof(struct FileAttributes));
      break;
    }
    case CREATE_FILE:
      ret = ops->createFile(&name, &fPath);
      break;
    case READ:
    {
      struct VfsReadArgs *args = (struct VfsReadArgs *)argStart;
      ret = ops->read(entry->device, &fPath, args, outBuffer);
      *outBytes = (ret < 0 ? 0 : ret);
      break;
    }
    case WRITE:
      ret = ops->write(&fPath, argStart, inBytes);
      break;
    case REMOVE:
      ret = ops->remove(&fPath);
      break;
    case LINK:
      ret = ops->link(&name, &fPath);
      break;

*/
