#ifndef PROGLOAD_H
#define PROGLOAD_H

#include <elf.h>

bool isValidElf32Exe( elf32_header_t *image );

#endif /* PROGLOAD_H */
