#ifndef UMPO_H
#define UMPO_H

#include <types.h>

int _sendto(mid_t port, mid_t reply, unsigned short subj, void *data,
size_t len, int dont_frag);

#endif /* UMPO_H */
