#ifndef _CONNECTION_H
#define _CONNECTION_H

struct ShmRegInfo
{
  char *address;
  size_t length;
  mid_t mbox;
};

shmid_t __connect(mid_t server_mbox, struct MemRegion *region, size_t pages);
int __disconnect(struct ShmRegInfo *);

#endif /* _CONNECTION_H */
