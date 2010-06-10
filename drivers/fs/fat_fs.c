#include <os/vfs.h>
#include <os/os_types.h>

extern SBAssocArray filesystems;

int fatListAttr( SBFilePath *path, struct FileAttributes **attrib )
{

}

int fatReadFile( SBFilePath *path, struct FileAttributes **attrib )
{

}

int fatReadFile( SBFilePath *path, char **buffer, size_t bytes )
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

  fs->ops.list = fatListAttr;
  fs->ops.read = fatReadFile;
  fs->ops.getAttributes = fatGetAttr;

  sbStringCreate(name, "fat", 1);
  memcpy(fs->name, "fat", 3);
  sbAssocArrayInsert(&filesystems, name, sizeof *name, fs, sizeof *fs);
  return 0;
}
