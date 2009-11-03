#include <fs.h>
#include <vfs.h>

void RegisterFS(struct filesystem *fs)
{
  struct filesystem *ptr;

  if(fs_list == NULL)
    fs_list = fs;
  else
  {
    for(ptr=fs_list; ptr->next != NULL; ptr = ptr->next);
    ptr->next = fs;
  }
}

struct filesystem *FindFS(char *name)
{
  struct filesystem *ptr;

  for(ptr=fs_list; ptr != NULL; ptr = ptr->next)
  {
    if(strcmp(ptr->name, name) == 0)
      break;
  }
  if(ptr == NULL)
    return NULL;
  else
    return ptr;
}

