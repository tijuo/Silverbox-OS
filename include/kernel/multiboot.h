#ifndef MBOOTHEAD
#define MBOOTHEAD

/* Macros.  */

/* The magic number for the Multiboot header.  */
#define MULTIBOOT_HEADER_MAGIC		0x1BADB002u

/* The flags for the Multiboot header.  */
#ifdef __ELF__
# define MULTIBOOT_HEADER_FLAGS		0x00000003u
#else
# define MULTIBOOT_HEADER_FLAGS		0x00010003u
#endif

/* The magic number passed by a Multiboot-compliant boot loader.  */
#define MULTIBOOT_BOOTLOADER_MAGIC	0x2BADB002u

/* C symbol format. HAVE_ASM_USCORE is defined by configure.  */
#ifdef HAVE_ASM_USCORE
# define EXT_C(sym)			_ ## sym
#else
# define EXT_C(sym)			sym
#endif

#define MULTIBOOT_MMAP_TYPE_RAM		1u

/* Types.  */

/* The Multiboot header.  */
typedef struct multiboot_header {
  unsigned int magic;
  unsigned int flags;
  unsigned int checksum;
  unsigned int header_addr;
  unsigned int load_addr;
  unsigned int load_end_addr;
  unsigned int bss_end_addr;
  unsigned int entry_addr;
} multiboot_header_t;

/* The symbol table for a.out.  */
typedef struct aout_symbol_table {
  unsigned int tabsize;
  unsigned int strsize;
  unsigned int addr;
  unsigned int reserved;
} aout_symbol_table_t;

/* The section header table for ELF.  */
typedef struct elf_section_header_table {
  unsigned int num;
  unsigned int size;
  unsigned int addr;
  unsigned int shndx;
} elf_section_header_table_t;

#define MBI_FLAGS_MEM            (1u << 0)  /* 'mem_*' fields are valid */
#define MBI_FLAGS_BOOT_DEV       (1u << 1)  /* 'boot_device' field is valid */
#define MBI_FLAGS_CMDLINE        (1u << 2)  /* 'cmdline' field is valid */
#define MBI_FLAGS_MODS           (1u << 3)  /* 'mods' fields are valid */
#define MBI_FLAGS_SYMTAB         (1u << 4)  /* 'syms.symtab' field is valid */
#define MBI_FLAGS_SHDR           (1u << 5)  /* 'syms.shdr' field is valid */
#define MBI_FLAGS_MMAP           (1u << 6)  /* 'mmap_*' fields are valid. */
#define MBI_FLAGS_DRIVES         (1u << 7)  /* 'drives_*' fields are valid */
#define MBI_FLAGS_CONFIG         (1u << 8)  /* 'config_table' field is valid */
#define MBI_FLAGS_BOOTLDR        (1u << 9)  /* 'boot_loader_name' field is valid */
#define MBI_FLAGS_APM_TAB        (1u << 10) /* 'apm_table' field is valid */
#define MBI_FLAGS_GFX_TAB        (1u << 11) /* Grahphics table is available */

/* The Multiboot information.  */
typedef struct multiboot_info {
  unsigned int flags;
  unsigned int mem_lower;
  unsigned int mem_upper;
  unsigned int boot_device;
  unsigned int cmdline;
  unsigned int mods_count;
  unsigned int mods_addr;
  union Tables {
    aout_symbol_table_t symtab;
    elf_section_header_table_t shdr;
  } syms;
  unsigned int mmap_length;
  unsigned int mmap_addr;
  unsigned int drives_length;
  unsigned int drives_addr;
  unsigned int config_table;
  unsigned int boot_loader_name;
  unsigned int apm_table;
  unsigned int vbe_control_info;
  unsigned int vbe_mode_info;
  unsigned short int vbe_mode;
  unsigned short int vbe_inferace_seg;
  unsigned short int vbe_interface_off;
  unsigned short int vbe_interface_len;
} multiboot_info_t;

/* The module structure.  */
typedef struct module {
  unsigned int mod_start;
  unsigned int mod_end;
  unsigned int string;
  unsigned int reserved;
} module_t;

#define MBI_TYPE_AVAIL  1   // Available RAM
#define MBI_TYPE_ACPI   3   // ACPI tables
#define MBI_TYPE_RESD   4   // Reserved RAM that must be preserved upon hibernation
#define MBI_TYPE_BAD    5   // Defective RAM

typedef struct memory_map {
  unsigned int size; // doesn't include itself
  unsigned int base_addr_low;
  unsigned int base_addr_high;
  unsigned int length_low;
  unsigned int length_high;
  unsigned int type;
} memory_map_t;

typedef struct apm_table {
  unsigned short int version;
  unsigned short int cseg;
  unsigned int offset;
  unsigned short int cseg_16;
  unsigned short int dseg;
  unsigned short int flags;
  unsigned short int cseg_len;
  unsigned short int cseg_16_len;
  unsigned short int dseg_len;
} apm_table_t;

typedef struct drive_info {
  unsigned int size;
  unsigned char drive_number;
  unsigned char drive_mode;
  unsigned short int drive_cylinders;
  unsigned char drive_heads;
  unsigned char drive_sectors;
  unsigned short int drive_ports[];
} drive_info_t;

#endif
