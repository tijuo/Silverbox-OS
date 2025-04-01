#ifndef PROGLOAD_H
#define PROGLOAD_H

#include <elf.h>

bool is_valid_elf_exe( elf_header_t *image );

#endif /* PROGLOAD_H */
