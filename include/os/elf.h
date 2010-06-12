#ifndef OS_ELF_H
#define OS_ELF_H

#include <elf.h>
#include <types.h>

bool isValidElfExe( elf_header_t *image );

#endif /* OS_ELF_H */
