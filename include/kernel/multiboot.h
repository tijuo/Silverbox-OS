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
typedef struct multiboot_header
{
  unsigned long magic;
  unsigned long flags;
  unsigned long checksum;
  unsigned long header_addr;
  unsigned long load_addr;
  unsigned long load_end_addr;
  unsigned long bss_end_addr;
  unsigned long entry_addr;
} multiboot_header_t;

/* The symbol table for a.out.  */
typedef struct aout_symbol_table
{
  unsigned long tabsize;
  unsigned long strsize;
  unsigned long addr;
  unsigned long reserved;
} aout_symbol_table_t;

/* The section header table for ELF.  */
typedef struct elf_section_header_table
{
  unsigned long num;
  unsigned long size;
  unsigned long addr;
  unsigned long shndx;
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
typedef struct multiboot_info
{
  unsigned long flags;
  unsigned long mem_lower;
  unsigned long mem_upper;
  unsigned long boot_device;
  unsigned long cmdline;
  unsigned long mods_count;
  unsigned long mods_addr;
  union Tables
  {
    aout_symbol_table_t symtab;
    elf_section_header_table_t shdr;
  } syms;
  unsigned long mmap_length;
  unsigned long mmap_addr;
  unsigned long drives_length;
  unsigned long drives_addr;
  unsigned long config_table;
  unsigned long boot_loader_name;
  unsigned long apm_table;
  unsigned long vbe_control_info;
  unsigned long vbe_mode_info;
  unsigned short vbe_mode;
  unsigned short vbe_inferace_seg;
  unsigned short vbe_interface_off;
  unsigned short vbe_interface_len;
} multiboot_info_t;

/* The module structure.  */
typedef struct module
{
  unsigned long mod_start;
  unsigned long mod_end;
  unsigned long string;
  unsigned long reserved;
} module_t;

/* The memory map. Be careful that the offset 0 is base_addr_low
   but no size.  */
typedef struct memory_map
{
  unsigned long size;
  unsigned long base_addr_low;
  unsigned long base_addr_high;
  unsigned long length_low;
  unsigned long length_high;
  unsigned long type;
} memory_map_t;

typedef struct apm_table
{
  unsigned short version;
  unsigned short cseg;
  unsigned long  offset;
  unsigned short cseg_16;
  unsigned short dseg;
  unsigned short flags;
  unsigned short cseg_len;
  unsigned short cseg_16_len;
  unsigned short dseg_len;
} apm_table_t;

struct drive_info
{
  unsigned long size;
  unsigned char drive_number;
  unsigned char drive_mode;
  unsigned short drive_cylinders;
  unsigned char drive_heads;
  unsigned char drive_sectors;
  unsigned short drive_ports[];
} drive_info_t;

#endif
