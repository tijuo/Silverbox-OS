#ifndef ELF_HEADER
#define ELF_HEADER

#include <stdint.h>

#define ET_NONE			0
#define ET_REL			1
#define ET_EXEC			2
#define ET_DYN			3
#define ET_CORE			4
#define ET_LOOS			0xFE00
#define ET_HIOS			0xFEFF
#define ET_LOPROC		0xFF00
#define ET_HIPROC		0xFFFF

#define PF_R			4
#define PF_W			2
#define PF_X			1

#define EM_NONE			0
#define EM_M32			1
#define EM_SPARC		2
#define EM_386			3
#define EM_68K			4
#define EM_88K			5
#define EM_860			7
#define EM_MIPS			8
#define EM_X86_64		62

#define EV_NONE			0
#define EV_CURRENT		1

#define EI_MAG0			0
#define EI_MAG1			1
#define EI_MAG2			2
#define EI_MAG3			3
#define EI_CLASS		4
#define EI_DATA			5
#define EI_VERSION		6
#define EI_OSABI		7
#define EI_ABIVERSION	8
#define EI_PAD			9
#define EI_NIDENT		16

#define ELFMAG0			0x7F
#define ELFMAG1			'E'
#define ELFMAG2			'L'
#define ELFMAG3			'F'

#define ELFCLASSNONE	0
#define ELFCLASS32		1
#define ELFCLASS64		2

#define ELFDATANONE		0
#define ELFDATA2LSB		1
#define ELFDATA2MSB		2

#define ELFOSABI_SYSV		0
#define ELFOSABI_HPUX		1
#define ELFOSABI_STANDALONE	255

#define SHN_UNDEF		0
#define SHN_LORESERVE	0xFF00
#define SHN_LOPROC		0xFF00
#define SHN_HIPROC		0xFF1F
#define SHN_ABS			0xFFF1
#define SHN_COMMON		0xFFF2
#define SHN_HIRESERVE		0xFFFF

#define SHT_NULL		0
#define SHT_PROGBITS		1
#define SHT_SYMTAB		2
#define SHT_STRTAB		3
#define SHT_RELA		4
#define SHT_HASH		5
#define SHT_DYNAMIC		6
#define SHT_NOTE		7
#define SHT_NOBITS		8
#define SHT_REL			9
#define SHT_SHLIB		10
#define SHT_DYNSYM		11
#define SHT_LOOS		0x60000000
#define SHT_HIOS		0x6FFFFFFF
#define SHT_LOPROC		0x70000000
#define SHT_HIPROC		0x7FFFFFFF
#define SHT_LOUSER		0x80000000
#define SHT_HIUSER		0xFFFFFFFF

#define SHF_WRITE		1
#define SHF_ALLOC		2
#define SHF_EXECINSTR		4
#define SHF_MASKOS			0x0F000000
#define SHF_MASKPROC		0xF0000000

#define STN_UNDEF		0

#define ELF32_ST_BIND(i)	((i)>>4)
#define ELF32_ST_TYPE(i)	((i)&0xF)
#define ELF32_ST_INFO(b,t)	(((b)<<4) + ((t)&0xF))

#define STB_LOCAL		0
#define STB_GLOBAL		1
#define STB_WEAK		2
#define STB_LOOS		10
#define STD_HIOS		12
#define STB_LOPROC		13
#define STB_HIPROC		15

#define STT_NOTYPE		0
#define STT_OBJECT		1
#define STT_FUNC		2
#define STT_SECTION		3
#define STT_FILE		4
#define STT_LOOS		10
#define STT_HIOS		12
#define STT_LOPROC		13
#define STT_HIPROC		15

#define R_386_NONE      	0
#define R_386_32       		1
#define R_386_PC32      	2
#define R_386_GOT32     	3
#define R_386_PLT32     	4
#define R_386_COPY      	5
#define R_386_GLOB_DAT  	6
#define R_386_JMP_SLOT  	7
#define R_386_RELATIVE  	8
#define R_386_GOTOFF    	9
#define R_386_GOTPC     	10

#define PT_NULL			0
#define PT_LOAD			1
#define PT_DYNAMIC		2
#define PT_INTERP		3
#define PT_NOTE			4
#define PT_SHLIB		5
#define PT_PHDR			6
#define PT_LOOS			0x60000000
#define PT_HIOS			0x6FFFFFFF
#define PT_LOPROC		0x70000000
#define PT_HIPROC		0x7FFFFFFF

#define	DT_NULL			0
#define DT_NEEDED		1
#define DT_PLTRELSZ		2
#define DT_PLTGOT		3
#define DT_HASH			4
#define DT_STRTAB		5
#define DT_SYMTAB		6
#define DT_RELA			7
#define DT_RELASZ		8
#define DT_RELAENT		9
#define DT_STRSZ		10
#define DT_SYMENT		11
#define DT_INIT			12
#define DT_FINI			13
#define DT_SONAME		14
#define DT_RPATH		15
#define DT_SYMBOLIC		16
#define DT_REL			17
#define DT_RELSZ		18
#define DT_RELENT		19
#define DT_PLTREL		20
#define DT_DEBUG		21
#define DT_TEXTREL		22
#define DT_JMPREL		23
#define DT_BIND_NOW		24
#define DT_INIT_ARRAY	25
#define DT_FINI_ARRAY	26
#define DT_INIT_ARRAYSZ	25
#define DT_FINI_ARRAYSZ	26

#define DT_LOOS			0x60000000
#define DT_HIOS			0x6FFFFFFF
#define DT_LOPROC		0x70000000
#define DT_HIPROC		0x7FFFFFFF

#define VALID_ELF(img)		({ __typeof__ (img) _img=(img); \
                                  (_img->identifier[EI_MAG0]  == ELFMAG0) \
				&& (_img->identifier[EI_MAG1] == ELFMAG1) \
                                && (_img->identifier[EI_MAG2] == ELFMAG2) \
				&& (_img->identifier[EI_MAG3] == ELFMAG3); })

#define OK			        0
#define INVALID_ELF		    1
#define INVALID_CLASS		2
#define INVALID_VERSION		3
#define INVALID_MACHINE		4
#define INVALID_EXEC		5
#define INVALID_DYNAMIC		6
#define INVALID_SHLIB		7

extern unsigned _GLOBAL_OFFSET_TABLE_[];

static inline unsigned int elf32_hash(const unsigned char *name) {
  unsigned int h = 0;
  unsigned int g;

  while(*name) {
	h = (h << 4) + *name++;
	g = h & 0xF0000000;

	if(g)
	  h ^= g >> 24;
	h &= ~g;
  }
  return h;
}

typedef struct {
  signed int tag;
  union {
	unsigned int val;
	unsigned int ptr;
  } d_un;
} elf32_dyn_t;

extern elf32_dyn_t ELF32_DYNAMIC[];

typedef struct {
  unsigned int type;
  unsigned int offset;
  unsigned int vaddr;
  unsigned int paddr;
  unsigned int filesz;
  unsigned int memsz;
  unsigned int flags;
  unsigned int align;
} elf32_pheader_t;

typedef struct {
  unsigned int offset;
  unsigned int info;
} elf32_rel_t;

typedef struct {
  unsigned int offset;
  unsigned int info;
  int addend;
} elf32_rela_t;

#define ELF_R_SYM(i)		((i)>>8)
#define ELF_R_TYPE(i)		((unsigned char)(i))
#define ELF_R_INFO(s,t)		(((s)<<8) + (unsigned char)(t))

typedef struct {
  unsigned int name;
  unsigned int value;
  unsigned int size;
  unsigned char info;
  unsigned char other;
  unsigned short int shndx;
} elf32_sym_t;

typedef struct {
  unsigned int name;
  unsigned int type;
  unsigned int flags;
  unsigned int addr;
  unsigned int offset;
  unsigned int size;
  unsigned int link;
  unsigned int info;
  unsigned int addralign;
  unsigned int entsize;
} elf32_sheader_t;

typedef struct {
  char identifier[EI_NIDENT];
  unsigned short int type;
  unsigned short int machine;
  unsigned int version;
  unsigned int entry;
  unsigned int phoff;
  unsigned int shoff;
  unsigned int flags;
  unsigned short int ehsize;
  unsigned short int phentsize;
  unsigned short int phnum;
  unsigned short int shentsize;
  unsigned short int shnum;
  unsigned short int shstrndx;
} elf32_header_t;

/*
 * ELF64
 */

#define ELF64_R_SYM(i)((i) >> 32)
#define ELF64_R_TYPE(i)((i) & 0xffffffffL)
#define ELF64_R_INFO(s, t)(((s) << 32) + ((t) & 0xffffffffL))

typedef struct {
  unsigned char identifier[EI_NIDENT];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint64_t entry;
  uint64_t phoff;
  uint64_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
} elf64_header_t;

typedef struct {
  uint32_t name;
  uint32_t type;
  uint64_t flags;
  uint64_t addr;
  uint64_t offset;
  uint64_t size;
  uint32_t link;
  uint32_t info;
  uint64_t addralign;
  uint64_t entsize;
} elf64_sheader_t;

typedef struct {
  uint32_t name;
  unsigned char info;
  unsigned char other;
  uint16_t shndx;
  uint64_t value;
  uint64_t size;
} elf64_sym_t;

typedef struct {
  uint64_t offset;
  uint64_t info;
} elf64_rel_t;

typedef struct {
  uint64_t offset;
  uint64_t info;
  int64_t addend;
} elf64_rela_t;

typedef struct {
  uint32_t type;
  uint32_t flags;
  uint64_t offset;
  uint64_t vaddr;
  uint64_t paddr;
  uint64_t filesz;
  uint64_t memsz;
  uint64_t align;
} elf64_pheader_t;

typedef struct {
  int64_t tag;

  union {
	uint64_t val;
	uint64_t ptr;
  } un;

} elf64_dyn_t;

extern elf64_dyn_t ELF64_DYNAMIC[];

static inline unsigned long elf64_hash(const unsigned char *name) {
  unsigned long h = 0;
  unsigned long g;

  while(*name) {
	h = (h << 4) + *name++;
	g = h & 0xf0000000;

	if(g)
	  h ^= g >> 24;
	h &= 0x0fffffff;
  }
  return h;
}

#endif
