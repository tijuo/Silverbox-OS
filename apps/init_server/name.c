#include <oslib.h>
#include <os/services.h>
#include <string.h>

#define MAX_NAME_RECS	256
#define MAX_NAME_LEN	18

struct NameRecord
{
  char name[MAX_NAME_LEN];
  size_t name_len;
  tid_t tid;
};

static struct NameRecord name_records[MAX_NAME_RECS];
static unsigned num_records = 0;

int register_name(char *name, size_t len, tid_t tid)
{
  if( len > MAX_NAME_LEN )
    return -1;
  else if( num_records >= MAX_NAME_RECS )
    return -1;

  memcpy(name_records[num_records].name, name, len);

  name_records[num_records].name_len = len;
  name_records[num_records].tid = tid;

  num_records++;

  return 0;
}

tid_t lookup_name(char *name, size_t len)
{
  for(int i=0; i < num_records; i++)
  {
    if( strncmp(name_records[i].name, name, len) == 0 )
      return name_records[i].tid;
  }

  return NULL_TID;
}
