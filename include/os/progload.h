#ifndef PROGLOAD_H
#define PROGLOAD_H

#include <elf.h>

bool isValidElfExe( elf_header_t *image );

#endif /* PROGLOAD_H */
