#ifndef NAME_H
#define NAME_H

#include <types.h>
#include <oslib.h>
#include <os/vfs.h>
#include <os/ostypes/hashtable.h>
#include <os/msg/message.h>
#include <os/msg/init.h>

#define MAX_NAME_RECS	256

void name_register(msg_t *request, msg_t *response);
void name_lookup(msg_t *request, msg_t *response);
void name_unregister(msg_t *request, msg_t *response);

#endif /* NAME_H */
