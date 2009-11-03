#include <kernel/vfs.h>
#include <kernel/buffer.h>
#include <drivers/fatfs.h>
#include <kernel/interrupt.h>
#include <kernel/memory.h>
#include <kernel/syscalls.h>
#include <string.h>

/*
int fat_list(struct vdir *parent, struct vfile **flist, struct vdir **dlist);
int fat_locate(char *name, struct vdir *parent, struct vfile **file, struct vdir **dir);
int fat_read_file(struct file *file, void *buffer, unsigned count);
*/

int root_list(struct vdir *parent, struct vfile **flist, struct vdir **dlist);
int root_locate(char *name, struct vdir *parent, struct vfile **file, struct vdir **dir);
int sys_open(char *path, int mode);

/*
struct dir_ops fat_dops = {
  list: fat_list,
  locate: fat_locate
};
*/
struct dir_ops root_dops = {
  list: root_list,
  locate: root_locate
};
/*
struct file_ops fat_fops = {
  read: fat_read_file
};
*/
int root_list(struct vdir *parent, struct vfile **flist, struct vdir **dlist)
{
  if(parent == NULL)
    return -EGEN;

  if(flist != NULL)
    *flist = parent->file_list;

  if(dlist != NULL)
    *dlist = parent->subdirs;

  return 0;
}

int root_locate(char *name, struct vdir *parent, struct vfile **file, struct vdir **dir)
{
  char *fname;
  struct vdir *dptr;
  struct vfile *fptr;

  if(parent == NULL)
    return -EGEN;

  if((file != NULL) && (dir != NULL))
    return -EGEN;
  else if((file == NULL) && (dir == NULL))
    return -EGEN;

  if(dir != NULL) {
    fname = name;
  /* Search the cache for the dir */
    for(dptr = parent->subdirs; dptr != NULL; dptr = dptr->next) {
    /* It's in the cache. There is no need to check the device */
      if(strcmp(fname, dptr->name) == 0) {
        *dir = dptr;
        return 0;
      }
    }
  }
  else {
    fname = name;
  /* Search the cache for the file */
    for(fptr = parent->file_list; fptr != NULL; fptr = fptr->next) {
    /* It's in the cache. There is no need to check the device */
      if(strcmp(fname, fptr->name) == 0) {
        *file = fptr;
        return 0;
      }
    }
  }
  return -1;
}

int sys_cd(char *path)
{
  print("syscd: %x\n", path);
  return vfs_cd(path);
}

int sys_ldir(char *path)
{
  print("sys_ldir: %x\n", path);
  return vfs_ldir(path);
}

int sys_cwd(char *path, int buflen)
{
  return vfs_cwd(path, buflen);
}

int sys_open(char *path, int mode)
{
  return vfs_open(path, mode);
}

int vfs_cd(char *path)
{
  struct vdir *dir;
  int error;

  print("vfs_cd: %x", path);
  print("@ %s\n", path);

  if(path == NULL)
    return 1;

  dir = convert_path(path, &error);
  if(error != 0)
    return error;

  if(vcd(dir->name, dir->parent) == NULL)
    return 1;

  return 0;
}

int vfs_ldir(char *path)
{
  struct vdir *dir;
  int error;

  print("ldir: %x ", path);
  print("@ %s\n", path);

  if(path == NULL)
    return 1;

  dir = convert_path(path, &error);

  if(error != 0)
    return error;

  vldir(dir);
  return 0;
}

int vfs_cwd(char *path, int buflen)
{
  struct vdir *vdir;
  vdir = (struct vdir *)vcwd();
  if(vdir == NULL)
    return -1;

  if((strlen(vdir->name) + 1) > buflen)
    return -1;

  memcpy(path, vdir->name, strlen(vdir->name) + 1);
  path[strlen(vdir->name)] = '\0';
//  *path = vdir->name;

  return (strlen(vdir->name) + 1);;
}

int vfs_open(char *path, int mode)
{
  struct vdir *dir = NULL, *dprev = NULL;
  char *fname = NULL;
  int start, handle, error;
  struct file *fptr = NULL;

  if(path == NULL)
    return -1;

  fname = (char *)kmalloc(128);
  if(fname == NULL)
    return -1; 

  dprev = (struct vdir *)vcwd();

  dir = convert_path(path, &error);
  if(error <= 0) {
    kfree(fname);
    return -1;
  }
  if(dir == NULL) {
    kfree(fname);
    return -1;
  }

  memset(fname, 0, 128);

  for(start=strlen(path); path[start - 1] != '/'; start--);
  strncpy(fname, &path[start], strlen(path) - start);

  vdir_cd(dir);
  fptr = vopen(fname, mode);
  if(fptr == NULL) {
    print("vopen failed.\n");
    kfree(fname);
    return -1;
  }
  vdir_cd(dprev);

  for(handle=0; handle < MAX_FHANDLES; handle++) {
    if(task_current->fhand[handle] == NULL)
      break;
  }
  
  if(handle >= MAX_FHANDLES) {
    print("Too many handles.\n");
    kfree(fname);
    return -1;
  }
  task_current->fhand[handle] = fptr;

  if(fptr->fops->open != NULL)
    fptr->fops->open(fptr, mode);
//  kfree(fname);
  return handle;
}

struct vdir *convert_path(char *path, int *error/*, struct vfile *filename*/)
{
  struct vdir *dprev = NULL, *ret_dir = NULL;
  int i, dpos = 0, length;
  char *dname, *endchar;

  if(path == NULL) {
    *error = -1;
    return NULL;
  }

  dname = (char *)kmalloc(128);
  if(dname == NULL) {
    *error = -1;
    return NULL;
  }

  dprev = (struct vdir *)vcwd();

  if(path[0] == '/') {
    vdir_cd(vfs_root);
    ret_dir = vfs_root;
    dpos++;
    if(path[1] == '\0') {
      *error = 0;
      return ret_dir;
    }  
  }
  while(1) {
    endchar = (char *)strchr(&path[dpos], (int)'/'); 

    if(endchar == NULL) {
      memset(dname, 0, 128);
      for(i=0; path[dpos] != '\0'; dpos++, i++)
        dname[i] = path[dpos];
      dname[i] = '\0';
      if(vcd(dname, vcwd()) == NULL) {
        vdir_cd(dprev);
        if(error != NULL)
          *error = 1;
        return ret_dir;
      }
      ret_dir = (struct vdir *)vcwd();
      vdir_cd(dprev);
      
      if(error != NULL)
        *error = 0;
      return ret_dir;
    }  
    length = (addr_t)endchar - ((addr_t)path + dpos);  
    memset(dname, 0, 128);
    strncpy(dname, &path[dpos], length);
    dname[length] = '\0';
    if(vcd(dname, vcwd()) == NULL) {
      vdir_cd(dprev);
      if(error != NULL)
        *error = 2;
      return ret_dir;
    }
    ret_dir = (struct vdir *)vcwd();
    dpos += length + 1;
  }
}

int init_vfs(void)
{
/*
  static struct FS_header nullroot;
  nullroot.next = NULL;
  fsheader_list = &nullroot;
  struct filesystem initfs = { "initfs", initfs_init, 0 };
   
  RegisterFS(&initfs);
*/

  vfs_root = vndir("/", NULL);
  vfs_device = vndir("devices", vfs_root);
  vfs_mount = vndir("mounted", vfs_root);
  
  vfs_mount->d_ops = vfs_device->d_ops = vfs_root->d_ops = &root_dops;
  
  return 0;   
}  

int Mount(char *fsname, int dev, char *mntpoint, int flags)
{
  struct vdir *root;
  struct filesystem *filesys;
  int fl, another;
  char fbuffer[100];
  char *test = "This is a test for my console device.\n";
  int k;

  memset(fbuffer, 0, 100);

 /* Mounts root dir on a device to a directory on the VFS */
  root = vndir(mntpoint, vfs_mount);

  filesys = FindFS(fsname);
  filesys->init(root, dev, flags);
//  root->fs->f_ops = &fat_fops;
  root->fs->device = dev;
//  root->d_ops = &fat_dops;
  root->fs->filesys = filesys;
  root->attrib |= FL_DIRROOT | FL_DIRMOUNT;
//  root->attrib = FL_DIRMOUNT;

  /* setup vdir struct */

  root->size = 0;
  root->used = 0;
  vfs_cwdir = root;
  fl = sys_open("/mounted/adrive/boot/menu.lst", RD_MODE);
  if(fl == -1)
    print("sys_open failed: %d\n", fl);
  sys_read(fl, fbuffer, 100);

  another = sys_open("/devices/cons00", WR_MODE);
  if(another < 0)
    print("Another problem....\n");
  print("handle: %d\n", another);
  sys_write(another, test, strlen(test));

  for(k=0; k < 100; k++)
    kputchar(fbuffer[k]);
//vldir(vcwd());
  return 0;
}

// RemoveFS();
/*
Unmount(fs)
ListDirectory()
OpenFile()
CloseFile()
ReadFile()
*/

/*
struct FS_header *FindFS_header(int dev)
{
  struct FS_header *ptr;

  for(ptr=fsheader_list; ptr != NULL; ptr=ptr->next) {
    if(ptr->device == dev)
        break;
      else
        continue;
  }
  if(ptr == NULL)
    return NULL;
  else
    return ptr;
}
*/
struct vdir *vndir(char *name, struct vdir *parent)
{
  struct vdir *ndir, *ptr;
//  struct vfile *backlink, *currlink;

  if((parent == NULL) && (vfs_root != NULL))
    return NULL;

  ndir = (struct vdir *)kmalloc(sizeof(struct vdir));

  ndir->name = (char *)kmalloc(strlen(name) + 1);
  strcpy(ndir->name, name);
  ndir->name[strlen(name)] = '\0';
  ndir->attrib = 0;

  ndir->parent = parent;
  ndir->next = NULL;
  ndir->file_list = NULL;
  ndir->subdirs = NULL;
  
  ndir->d_ops = NULL;
  ndir->fs_info = NULL;
  
  ndir->blk_count = 0;
  ndir->blk_size = 0;
  ndir->blk_start = 0;

  if(parent == NULL) {
    ndir->prev = NULL;
    vfs_root = ndir;
    ndir->parent = ndir; // ???
  }
  else {
    if(parent->subdirs == NULL) {
      parent->subdirs = ndir;
      ndir->prev = NULL;
    }
    else {
      for(ptr = parent->subdirs; ptr->next != NULL; ptr = ptr->next);
      ndir->prev = ptr;
      ptr->next = ndir;
    }
  }
/*
  if((parent != NULL)  && (croot == NULL)) { / *  Providing dir links to the RAMFS only * / 
    currlink = (struct vfile *)kmalloc(sizeof(struct vfile));
    backlink = (struct vfile *)kmalloc(sizeof(struct vfile));

    currlink->parent = backlink->parent = ndir;
    currlink->name = (char *)kmalloc(2);
    backlink->name = (char *)kmalloc(3);

    currlink->name[0] = '.';
    currlink->name[1] = '\0';
    backlink->name[0] = backlink->name[1] = '.';
    backlink->name[2] = '\0';
    currlink->attrib = backlink->attrib = DIRECTORY | LINK;

    currlink->dir_link = ndir;
    backlink->dir_link = ndir->parent;

    currlink->prev = NULL;
    currlink->next = backlink;
    backlink->prev = currlink;
    backlink->next = NULL;

    ndir->file_list = currlink;
  }
*/
  return ndir;    
}  

struct file *vopen(char *name, int mode)
{
  struct vfile *vfile = NULL;
  struct file *file = NULL;
  int error;
  struct vdir *current = (struct vdir *)vcwd();
  
  /*
  for(ptr=vfs_cwdir->file_list; ptr != NULL; ptr = ptr->next)  {
    if(strcmp(ptr->name, name) == 0)
      break;
  }
  
  if(ptr == NULL)
    return NULL;
  */

  if(current->d_ops->locate == NULL)
    return NULL;

  error = current->d_ops->locate(name, current, &vfile, NULL);

  if((error < 0) || (vfile == NULL)) {
    print("Couldn't find %s\n", name);
    return NULL;
  }
  file = (struct file *)kmalloc(sizeof(struct file));
  if(file == NULL)
    return NULL;
    
  file->vfile = vfile;

  if((vfile->attrib & ATR_DEVICE) == ATR_DEVICE)
    file->dev_num = vfile->dev_num;
  else
    file->dev_num = vfile->fs->device;

  file->pos = 0;
  file->mode = mode;
  file->lock = 0;
  file->size = vfile->size;
  file->flags = 0;
  file->fops = vfile->f_ops;

  return file;
}

struct vfile *vnfile(char *name, struct vdir *parent)
{
  struct vfile *nfile, *ptr;

  if((parent == NULL) || (vfs_root == NULL))
    return NULL;

  nfile = (struct vfile *)kmalloc(sizeof(struct vfile));

  nfile->name = (char *)kmalloc(strlen(name) + 1);
  strcpy(nfile->name, name);
  nfile->name[strlen(name)] = '\0';
  nfile->attrib = 0;
//  nfile->date = nfile->time = 0;
  nfile->parent = parent;
  nfile->next = NULL;
  nfile->f_ops = NULL;

  nfile->blk_count = 0;
  nfile->blk_size = 0;
  nfile->blk_start = 0;

  if(parent == NULL) {
    kfree(nfile);
    kfree(nfile->name);
    return NULL;
  }
  else {
    if(parent->file_list == NULL) {
      parent->file_list = nfile;
      nfile->prev = NULL;
    }
    else {
      for(ptr = parent->file_list; ptr->next != NULL; ptr = ptr->next);
      nfile->prev = ptr;
      ptr->next = nfile;
    }
  }
  return nfile;
}

struct vdir *vcd(char *name, struct vdir *parent/*, int *error*/)
{
  struct vdir *subdir = NULL;
  int result;
  
  if(parent == NULL) {
    if(strcmp(name, "/") == 0) {
      vfs_cwdir = vfs_root;
      return vfs_root;
    }
    else
      return NULL;
  }
  if(parent->d_ops->locate == NULL)
    return NULL;

  result = parent->d_ops->locate(name, parent, NULL, &subdir);
  if((result < 0) || (subdir == NULL))
    return NULL;
  vfs_cwdir = subdir; 
  return subdir;
}

int vdir_cd(struct vdir *dir)
{
  if(dir == NULL)
    return -1;
  vfs_cwdir = dir;
  return 0;
}

struct vdir *vcwd(void)
{
  return vfs_cwdir;
}

void vldir(struct vdir *wd)
{
  struct vdir  *dir_ptr;
  struct vfile *file_ptr;

  dir_ptr = NULL;
  file_ptr = NULL;

  wd->d_ops->list(wd, &file_ptr, &dir_ptr);

  if((wd->file_list == NULL) && (file_ptr != NULL))
    wd->file_list = file_ptr;
  if((wd->subdirs == NULL) && (dir_ptr != NULL))
    wd->subdirs = dir_ptr;

  print("Listing contents of: %s\n\n", wd->name);

  while(dir_ptr != NULL) {
    print("%s\t%30x\n", dir_ptr->name, dir_ptr->attrib);
    dir_ptr = dir_ptr->next;
  }

  while(file_ptr != NULL) {
    print("%s\t%30x\n", file_ptr->name, file_ptr->attrib);
    file_ptr = file_ptr->next;
  }
  print("\n");
}
