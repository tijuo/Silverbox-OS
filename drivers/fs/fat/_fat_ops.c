#include <kernel/vfs.h>
#include <kernel/buffer.h>
#include <drivers/fatfs.h>
#include <kernel/interrupt.h>

int fatListDir( char *path, int flags );
int fatReadFile( char *path, int flags );
int fatWriteFile( char *path, int flags );
int fatFsctl( int cmd, ... );
int fatCreateFile( char *path, int attrib );
int fatCreateDir( char *path, int attrib );
int fatModifyAttrib( int attrib, ... );
int fatCreateLink( char *path, char *linkPath );
int fatDeleteFile( char *path );
int fatDeleteDir( char *path );
int fatDeleteLink( char *path, int flags );

int fat_list(struct vdir *parent, struct vfile **flist, struct vdir **dlist);
int fat_locate(char *name, struct vdir *parent, struct vfile **file, struct vdir **dir);
int fat_read_file(struct file *file, void *buffer, unsigned count);

struct dir_ops fat_dops = {
  list: fat_list,
  locate: fat_locate
};

struct file_ops fat_fops = {
  read: fat_read_file
};

int fat_read_file(struct file *file, void *buffer, unsigned count)
{
  int i, blk, left;
  struct buffer *b;
  struct FAT_dev *fat_dev;
  word clus_dat;

  if((file == NULL) || (buffer == NULL))
    return -1;
  if((file->flags & FL_EOF) == FL_EOF)
    return 0;

  fat_dev = (struct FAT_dev *)file->vfile->fs_info;

  blk = file->vfile->blk_start;
  clus_dat = read_fat(fat_dev, BLKTOCLUSTER(file->vfile->blk_start, fat_dev->info, 1));

  if(clus_dat == 0xFF8) {
    file->flags |= FL_EOF;
    return 0;
  }

  left = count;

  for(i=0; i < count; ) {
    bnew(file->dev_num, blk); 
    b = (struct buffer *)bread(file->dev_num, blk);
    if((file->pos + file->vfile->blk_size - 
      (file->pos % file->vfile->blk_size)) >= file->size) {
       memcpy(buffer + i, b->data +  (file->pos % file->vfile->blk_size),
             file->size - file->pos);
       file->flags |= FL_EOF;
       return (i + file->size - file->pos);
    }
    else if(left <= (file->vfile->blk_size - (file->pos % file->vfile->blk_size))) {      
      memcpy(buffer + i, b->data + (file->pos % file->vfile->blk_size), left);
      return (i + left);
    }
    else {
      memcpy(buffer + i, b->data + (file->pos % file->vfile->blk_size),
             file->vfile->blk_size - (file->pos % file->vfile->blk_size));
      left -= file->vfile->blk_size - (file->pos % file->vfile->blk_size);
      i += file->vfile->blk_size - (file->pos % file->vfile->blk_size);
    }
    clus_dat = read_fat(fat_dev, clus_dat);
    if(clus_dat == 0xFF8) {
      file->flags |= FL_EOF;
      return i;
    }
    blk = CalcFirstSecClus(clus_dat, &fat_dev->info);
  }
  return -1;
}

int fat_locate(char *name, struct vdir *parent, struct vfile **file, struct vdir **dir)
{
  char attrib, filename[12], *fname;
  struct vdir *dptr;
  struct vfile *fptr = NULL;
  struct FAT_dir_entry entry;
  struct FAT_dev *fat_dev = (struct FAT_dev *)parent->fs_info;
  int i, j;
  int bytes_per_sec, secs_per_clus, blknum;
  struct buffer *b;
  struct FS_header *fs = parent->fs;
  
  if(parent == NULL)
    return -EGEN;
  
  if((file != NULL) && (dir != NULL))
    return -EGEN;
  else if((file == NULL) && (dir == NULL))
    return -EGEN;
    
  /*fname = (char *)malloc(12);
  if(fname == NULL)  
    return -1;
    
  if(strlen(name) > 11)
    strncpy(fname, name, 11);
  else {
    strncpy(fname, name, strlen(name));
    memset((void *)((addr_t)fname + strlen(name)), ' ', 11 - strlen(name));
  }
  fname[12] = '\0';
  */
  if(dir != NULL) {
    fname = (char *)parse_dirname(name);
    if(fname == NULL)
      return -1;
  /* Search the cache for the dir */
    for(dptr = parent->subdirs; dptr != NULL; dptr = dptr->next) {
    /* It's in the cache. There is no need to check the device */
      if(strcmp(fname, dptr->name) == 0) {
        *dir = dptr;
        return 0;
      }
      else if((parent->attrib & FL_DIRCACHE) == FL_DIRCACHE)
        return -EBUG; /* This is never supposed to happen. */ 
                     /* If if does, then the cache isn't
                        working properly */
    }  
  }
  else {
    fname = (char *)parse_filename(name);
  /* Search the cache for the file */
    for(fptr = parent->file_list; fptr != NULL; fptr = fptr->next) {
    /* It's in the cache. There is no need to check the device */
      if(strcmp(fname, fptr->name) == 0) {
        *file = fptr;
        return 0;
      }
      else if((parent->attrib & FL_DIRCACHE) == FL_DIRCACHE) 
        return -EBUG; /* This is never supposed to happen. */
                     /* If if does, then the cache isn't
                      working properly */
    }
  }

  if(fat_dev->cache.fat_type == 0) {
    bytes_per_sec = fat_dev->info.fat12.bytes_per_sec;
    secs_per_clus = fat_dev->info.fat12.secs_per_clus;
    i = fat_dev->info.fat12.root_ents;
  }
  else if(fat_dev->cache.fat_type == 1) {
    bytes_per_sec = fat_dev->info.fat16.bytes_per_sec;
    secs_per_clus = fat_dev->info.fat16.secs_per_clus;
    i = fat_dev->info.fat16.root_ents;
  }
/*
  if((parent->attrib & FL_DIRROOT) != FL_DIRROOT)
    blknum = parent->blk_start;// * bytes_per_sec * secs_per_clus;
  else
*/
    blknum = parent->blk_start;// * bytes_per_sec;

  while(i > 0) {
    bnew(fs->device, blknum);
    b = (struct buffer *)bread(fs->device, blknum);
    if(b == NULL)
      return -EGEN;
    for(j=0; j < (bytes_per_sec / sizeof(struct FAT_dir_entry)); j++, i--) {
      memcpy(&entry, (void *)((addr_t)b->data + j * sizeof(struct FAT_dir_entry)),
             sizeof(struct FAT_dir_entry));
      memcpy(filename, &entry.filename, 11);

      filename[11] = '\0';
      if(filename[0] == (char)0)
        return 0;
      if(filename[0] == (char)0xE5) /* Disregard deleted file entries */
        continue;
      if(entry.start_clus == 0)     /* The start cluster should never be 0. */
        continue;                   /* This file is probably storing a long filename */
      if(strncmp(filename, fname, 11) == 0) {
        if(((entry.attrib & 0x10) == 0x10) && (dir != NULL)) { /* Is it a directory? */
          dptr = vndir(filename, parent);
          dptr->d_ops = parent->d_ops;
          dptr->blk_start = CalcFirstSecClus(entry.start_clus, &fat_dev->info);
          dptr->blk_size  = bytes_per_sec;// * secs_per_clus;
          dptr->fs_info = parent->fs_info;
          dptr->fs = parent->fs;
          dptr->size = 0;
          dptr->root = parent->fs->root;
          dptr->attrib = 0;
          *dir = dptr;
        }  
        else if(((entry.attrib & 0x10) == 0) && (file != NULL)) {
          fptr = vnfile(filename, parent);
          fptr->blk_start = CalcFirstSecClus(entry.start_clus, &fat_dev->info);
          fptr->blk_size  = bytes_per_sec;// * secs_per_clus;
          fptr->size = entry.file_size;
          fptr->fs_info = parent->fs_info;
          fptr->fs = parent->fs;
          fptr->root = parent->fs->root;
          fptr->f_ops = parent->fs->f_ops;
          fptr->attrib = 0;
          *file = fptr;
        }
        else
          return -EGEN;
          
        return 0;         
      }
    }
  }
  return -EGEN;
}

int fat_list(struct vdir *parent, struct vfile **flist, struct vdir **dlist)
{
  char attrib, filename[12];
  struct vdir *dptr;
  struct vfile *fptr;
  struct FAT_dir_entry entry;
  struct FAT_dev *fat_dev = (struct FAT_dev *)parent->fs_info;
  int i, j, entry_found;
  int bytes_per_sec, secs_per_clus, blknum;
  struct buffer *b;
  struct FS_header *fs = parent->fs;

  if(parent == NULL)
    return -EGEN;

  if(fat_dev->cache.fat_type == 0) {
    bytes_per_sec = fat_dev->info.fat12.bytes_per_sec;
    secs_per_clus = fat_dev->info.fat12.secs_per_clus;
    i = fat_dev->info.fat12.root_ents;
  }
  else if(fat_dev->cache.fat_type == 1) {
    bytes_per_sec = fat_dev->info.fat16.bytes_per_sec;
    secs_per_clus = fat_dev->info.fat16.secs_per_clus;
    i = fat_dev->info.fat16.root_ents;
  }

  if((parent->attrib & FL_DIRROOT) != FL_DIRROOT)
    blknum = parent->blk_start;// * bytes_per_sec * secs_per_clus;
  else
    blknum = parent->blk_start;// * bytes_per_sec;

  while(i > 0) {
    bnew(fs->device, blknum);
    b = (struct buffer *)bread(fs->device, blknum);
    if(b == NULL)
      return -1;
    for(j=0; j < (bytes_per_sec / sizeof(struct FAT_dir_entry)); j++, i--) {
      memcpy(&entry, (void *)((addr_t)b->data + j * sizeof(struct FAT_dir_entry)),
             sizeof(struct FAT_dir_entry));
      memcpy(filename, &entry.filename, 11);

      filename[11] = '\0';

      if(filename[0] == (char)0) {
        i = 0;
        break;
      }
    
      if(filename[0] == (char)0xE5) /* Disregard deleted file entries */
        continue;

      entry_found = 0;

      if((entry.attrib & 0x10) == 0x10) { /* Is it a directory? */
        for(dptr = parent->subdirs; dptr != NULL; dptr = dptr->next) {
          if(strncmp(filename, dptr->name, 11) == 0) {
            entry_found = 1;
            break;
          }
        }

        if((dptr == NULL) || (!entry_found)) {
          dptr = vndir(filename, parent);
          dptr->d_ops = parent->d_ops;
          dptr->blk_start = CalcFirstSecClus(entry.start_clus, &fat_dev->info);
          dptr->blk_size  = bytes_per_sec;// * secs_per_clus;
          dptr->fs_info = parent->fs_info;
          dptr->fs = parent->fs;
          dptr->attrib = 0;
          dptr->size = 0;
        }
        if(*dlist == NULL)
          *dlist = parent->subdirs;
        
      /* This isn't the first time that we did a list(), so return */
/*
        else {
          dlist = dir->dir_list;
          flist = dir->file_list;
        }
*/
      }
      
      else {
        for(fptr = parent->file_list; fptr != NULL; fptr = fptr->next) {
          if(strncmp(filename, dptr->name, 11) == 0) {
            entry_found = 1;
            break;
          }
        }
        if((fptr == NULL) || (!entry_found)) {
          fptr = vnfile(filename, parent);
          fptr->blk_start = CalcFirstSecClus(entry.start_clus, &fat_dev->info);
          fptr->blk_size  = bytes_per_sec;// * secs_per_clus;
          fptr->fs_info = parent->fs_info;
          fptr->fs = parent->fs;
          fptr->size = entry.file_size;
          fptr->f_ops = parent->fs->f_ops;
          fptr->attrib = 0;
        }
        if(*flist == NULL)
          *flist = parent->file_list;
      }
      
    }
  }
//  parent->attrib |= FL_DIRCACHE;
  return 0;
}
