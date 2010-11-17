#include <os/vfs.h>
#include <os/os_types.h>

extern SBAssocArray filesystems;

int fatList( SBFilePath *path, struct FileAttributes **attrib )
{
  // then use the relative path for the fat operations
}

int fatGetAttr( SBFilePath *path, struct FileAttributes **attrib )
{

}

int fatRead( SBFilePath *path, char **buffer, size_t bytes )
{

}

int registerFAT(void)
{
  struct Filesystem *fs = calloc(1, sizeof (struct Filesystem));
  struct SBString *name = malloc(sizeof (SBString));

  if( !fs || ! name )
  {
    free(fs);
    free(name);

    return -1;
  }

  fs->ops.list = fatList;
  fs->ops.read = fatRead;
  fs->ops.getAttributes = fatGetAttr;

  sbStringCreate(name, "fat", 1);
  memcpy(fs->name, "fat", 3);
  sbAssocArrayInsert(&filesystems, name, sizeof *name, fs, sizeof *fs);
  return 0;
}
