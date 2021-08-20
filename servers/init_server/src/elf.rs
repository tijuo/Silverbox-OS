use crate::address::{PAddr, PSize};
use crate::lowlevel::phys;
use core::{mem, cmp};
use alloc::vec::Vec;
use alloc::boxed::Box;
use alloc::prelude::v1::{ToString, String};

#[repr(packed(1))]
#[derive(Default, Clone, Copy)]
pub struct RawElfHeader {
    identifier: [u8; 16],
    elf_type: u16,
    machine: u16,
    version: u32,
    entry: u32,
    phoff: u32,
    shoff: u32,
    flags: u32,
    ehsize: u16,
    phentsize: u16,
    phnum: u16,
    shentsize: u16,
    shnum: u16,
    shstrndx: u16
}

impl RawElfHeader {
    pub const ELF_MAGIC: [u8; 4] = [0x7f, 0x45, 0x4c, 0x46];
    pub const EI_VERSION: usize = 6;
    pub const EI_CLASS: usize = 4;
    pub const EM_386: u16 = 3;
    pub const EV_CURRENT: u8 = 1;
    pub const ET_EXEC: u16 = 2;
    pub const ELFCLASS32: u8 = 1;
}

#[repr(packed(4))]
#[derive(Default, Clone, Copy)]
pub struct RawMultibootHeader {
    pub magic: u32,
    pub flags: u32,
    pub checksum: u32,
    pub header_addr: u32,
    pub load_addr: u32,
    pub load_end_addr: u32,
    pub bss_end_addr: u32,
    pub entry_addr: u32,
}

/* The symbol table for a.out.  */
#[repr(packed(4))]
#[derive(Default, Copy, Clone)]
pub struct RawAOutSymbolTable {
    pub tabsize: u32,
    pub strsize: u32,
    pub addr: u32,
    pub reserved: u32,
}

/* The section header table for ELF.  */
#[repr(packed(4))]
#[derive(Default, Copy, Clone)]
pub struct RawSectionHeaderTable {
    pub num: u32,
    pub size: u32,
    pub addr: u32,
    pub shndx: u32,
}

#[repr(packed(4))]
#[derive(Copy, Clone)]
pub union RawTables {
    pub symtab: RawAOutSymbolTable,
    pub shdr: RawSectionHeaderTable,
}

impl Default for RawTables {
    fn default() -> Self {
        RawTables {
            shdr: RawSectionHeaderTable::default(),
        }
    }
}

#[repr(packed(1))]
#[derive(Copy, Clone, Default)]
pub struct RawDrive {
    pub size: u32,
    pub number: u8,
    pub mode: u8,
    pub cylinders: u16,
    pub heads: u8,
    pub sectors: u8,
}

impl RawDrive {
    pub const CHS: u8 = 0;
    pub const LBA: u8 = 1;
}

#[derive(Copy, Clone)]
pub enum MmapType {
    Available,
    Acpi,
    Reserved,
    Bad,
}

#[derive(Clone)]
pub struct SymbolTable {

}

#[derive(Clone)]
pub struct SectionHeaderTable {

}

#[derive(Copy, Clone)]
pub enum DriveMode {
    CHS,
    LBA,
}

#[derive(Clone)]
pub struct Drive {
    pub number: u8,
    pub cylinders: u16,
    pub heads: u8,
    pub sectors: u8,
    pub mode: DriveMode,
    pub io_ports: Vec<u16>,
}

fn read_c_str(addr: PAddr) -> Option<String> {
    if addr == 0 {
        None
    } else {
        let mut bytes_vec = Vec::new();
        let mut offset = 0;
        let mut str_option = None;

        'outer: loop {
            let mut chars = [0u8; 32];
            let result = phys::peek((addr + offset) as PAddr, &mut chars);

            if result.is_ok()  {
                for c in &chars {
                    if c == &0 {
                        str_option = String::from_utf8(bytes_vec).ok();
                        break 'outer;
                    } else {
                        bytes_vec.push(*c);
                    }
                }

                offset += chars.len() as PSize;
            } else {
                break;
            }
        }

        str_option
    }
}

pub fn read_multiboot_info(addr: PAddr) -> Option<Box<MultibootInfo>> {
    //let mut raw_info = RawMultibootInfo::default();
    let mut multiboot_info = Box::new(MultibootInfo::default());

    eprintln!("Reading multiboot struct...");

    let raw_info = if let Some(i) =
            unsafe { phys::try_from_phys::<RawMultibootInfo>(addr) } {
        eprintln!();
        i
    } else {
        eprintln!("failed.");
        return None;
    };

    eprint!("Reading mem...");

    multiboot_info.mem_size = if raw_info.flags & RawMultibootInfo::MEM == RawMultibootInfo::MEM {
        let lower = raw_info.mem_lower;
        let upper = raw_info.mem_upper;
        eprintln!("lower: {:#x} upper: {:#x}", lower * 1024, upper * 1024);
        Some(MemorySize {
            lower: lower as u64 * 1024,
            upper: upper as u64 * 1024
        })
    } else {
        eprintln!("NONE.");

        None
    };

    eprint!("Reading boot device...");

    multiboot_info.boot_device = if raw_info.flags & RawMultibootInfo::BOOT_DEV == RawMultibootInfo::BOOT_DEV {
        let drive = (raw_info.boot_device >> 24) as u8;
        let partition1 = ((raw_info.boot_device >> 16) & 0xFF) as u8;
        let partition2 = ((raw_info.boot_device >> 8) & 0xFF) as u8;
        let partition3 = (raw_info.boot_device & 0xFF) as u8;
        let dev_name = if drive < 0x80 { "fd" } else { "hd" };

        eprintln!("{}{}{}{}",
                  dev_name,
                  if partition1 == 0xFF { String::from("") } else { partition1.to_string() },
                  if partition2 == 0xFF { String::from("") } else { format!("-{}", partition2) },
                  if partition3 == 0xFF { String::from("") } else { format!(".{}", partition3) });

        Some(BootDevice {
            drive,
            partition1,
            partition2,
            partition3,
        })
    } else {
        eprintln!("NONE.");
        None
    };

    eprint!("Reading command line...");

    multiboot_info.command_line = if raw_info.flags & RawMultibootInfo::CMDLINE == RawMultibootInfo::CMDLINE {
        let cmdline = read_c_str(raw_info.cmdline as PAddr);

        match &cmdline {
            Some(s) => eprintln!("\"{}\"", s),
            None => eprintln!("NONE."),
        }

        cmdline
    } else {
        eprintln!("NONE.");
        None
    };

    eprint!("Reading modules...");

    multiboot_info.modules = if raw_info.flags & RawMultibootInfo::MODS == RawMultibootInfo::MODS {
        let total_mods = raw_info.mods_count;
        eprintln!("{} modules found.", total_mods);

        let mut modules: Vec<BootModule> = Vec::with_capacity(raw_info.mods_count as usize);

        for i in 0..raw_info.mods_count {
            unsafe { phys::try_from_phys_arr::<RawModule>(raw_info.mods_addr as PAddr, i as PSize) }
                .map_or_else(
                    || eprintln!("Unable to read boot module. Skipping..."),
                    |raw_module| {
                        let name = read_c_str(raw_module.string as PAddr);
                        let start = raw_module.mod_start;
                        let end = raw_module.mod_end;

                        if let Some(ref s) = name {
                            eprintln!("Module {}: \"{}\" addr: {:#x} size: {:#x} bytes",
                                      i, s, start, end - start);
                        }
                        modules.push(BootModule {
                            addr: raw_module.mod_start as PAddr,
                            length: (raw_module.mod_end - raw_module.mod_start) as PSize,
                            name
                        })
                    })

        }

        Some(modules)
    } else {
        eprintln!("NONE.");
        None
    };

    eprint!("Reading memory map...");

    multiboot_info.memory_map = if raw_info.flags & RawMultibootInfo::MMAP == RawMultibootInfo::MMAP {
        let mut offset= 0;
        //let mut mmap = RawMemoryMap::default();
        let mut mmap_vec = Vec::with_capacity(raw_info.mmap_length as usize / mem::size_of::<RawMemoryMap>());
        eprintln!();
        
        while offset < raw_info.mmap_length {
            match unsafe { phys::try_from_phys::<RawMemoryMap>((raw_info.mmap_addr +
                if offset == 0 { 0 } else { offset as u32 }) as PAddr) } {

                Some(mmap) => {
                    let mmap_type = match mmap.map_type {
                        RawMemoryMap::AVAILABLE => "Available",
                        RawMemoryMap::ACPI => "ACPI",
                        RawMemoryMap::BAD => "Bad",
                        RawMemoryMap::RESERVED => "Reserved",
                        _ => "Unknown",
                    };

                    let t = mmap.map_type;
                    let base_addr = mmap.base_addr;
                    let mmap_len = mmap.length;
                    let mmap_size = mmap.size;

                    eprintln!("{} region ({}) - addr: {:#x} size: {:#x}", mmap_type, t, base_addr, mmap_len);

                    mmap_vec.push(MemoryMap {
                        base_addr: mmap.base_addr as PAddr,
                        length: mmap.length as PSize,
                        map_type: match mmap.map_type {
                            RawMemoryMap::AVAILABLE => MmapType::Available,
                            RawMemoryMap::ACPI => MmapType::Acpi,
                            RawMemoryMap::RESERVED => MmapType::Reserved,
                            RawMemoryMap::BAD => MmapType::Bad,
                            _ => MmapType::Reserved,
                        },
                    });

                    offset += mmap.size + mem::size_of_val(&mmap_size) as u32;
                },
                None => {
                    eprintln!("Unable to read memory map record. Skipping...");
                    break;
                }
            };
        }
        Some(mmap_vec)
    } else {
        eprintln!("NONE.");
        None
    };

    eprint!("Reading drives...");

    multiboot_info.drives = if raw_info.flags & RawMultibootInfo::DRIVES == RawMultibootInfo::DRIVES {
        let mut offset= 0;
        let mut drives_vec = Vec::new();
        eprintln!();

        while offset < raw_info.drives_length {
            if let Some(drive) = unsafe { phys::try_from_phys::<RawDrive>(raw_info.drives_addr as PAddr + offset as PSize) } {
                let mut ports_vec = Vec::new();
                let mut port_offset = 0;

                let drive_mode = match drive.mode {
                    RawDrive::CHS => "CHS",
                    RawDrive::LBA => "LBA",
                    _ => "unknown"
                };

                let drive_number = drive.number;
                let drive_cylinders = drive.cylinders;
                let drive_heads = drive.heads;
                let drive_sectors = drive.sectors;

                eprintln!("Drive: {:#x} cylinders: {} heads: {} sectors: {} mode: {}", drive_number, drive_cylinders, drive_heads, drive_sectors, drive_mode);
                eprint!("  Ports:");

                'port_loop: loop {
                    let mut ports = [0u16; 64];

                    let result = unsafe {
                        phys::peek_ptr((addr + offset as PSize + port_offset) as PAddr,
                                       &mut ports,
                                       cmp::min(mem::size_of_val(&ports),
                                                drive.size as usize - (offset as usize + port_offset as usize)))
                    };

                    if result.is_ok() {
                        for p in &ports {
                            if p == &0 {
                                break 'port_loop;
                            } else {
                                eprint!(" {:#x}", *p);
                                ports_vec.push(*p);
                            }
                        }

                        port_offset += mem::size_of_val(&ports) as PSize;
                    } else {
                        break;
                    }
                }

                eprintln!();

                drives_vec.push(Drive {
                    number: drive.number,
                    cylinders: drive.cylinders,
                    heads: drive.heads,
                    sectors: drive.sectors,
                    mode: match drive.mode {
                        RawDrive::CHS => DriveMode::CHS,
                        RawDrive::LBA => DriveMode::LBA,
                        _ => {
                            eprintln!("Unsupported drive mode. Skipping...");
                            continue;
                        }
                    },
                    io_ports: ports_vec,
                });

                let drive_size = drive.size;

                offset += drive.size + mem::size_of_val(&drive_size) as u32;
            }
            else {
                eprintln!("Unable to read drive record. Skipping...");
                break;
            }
        }
        Some(drives_vec)
    } else {
        eprintln!("NONE.");
        None
    };

    // TODO: RawMultibootInfo::SYMTAB, RawMultibootInfo::SHDR

    eprint!("Reading config table...");
    let config_table = raw_info.config_table;

    multiboot_info.config_table = if raw_info.flags & RawMultibootInfo::CONFIG == RawMultibootInfo::CONFIG
        && raw_info.config_table != 0 {
        eprintln!("{:#x}", config_table);
        Some(raw_info.config_table as PAddr)
    } else {
        eprintln!("NONE.");
        None
    };

    eprint!("Reading bootloader name...");

    multiboot_info.bootloader_name = if raw_info.flags & RawMultibootInfo::BOOTLDR == RawMultibootInfo::BOOTLDR {
        let bootloader_name = read_c_str(raw_info.boot_loader_name as PAddr);

        match &bootloader_name {
            Some(s) => eprintln!("\"{}\"", s),
            None => eprintln!("NONE."),
        }

        bootloader_name
    } else {
        eprintln!("NONE.");
        None
    };

    eprint!("Reading APM table...");

    multiboot_info.apm_table = if raw_info.flags & RawMultibootInfo::APM_TAB == RawMultibootInfo::APM_TAB
        && raw_info.apm_table != 0 {
        eprintln!();

        unsafe { phys::try_from_phys::<ApmTable>(raw_info.apm_table as PAddr) }
            .map(|apm_table| {
                let version = apm_table.version;
                let cseg = apm_table.cseg;
                let cseg_len = apm_table.cseg_len;
                let cseg_16 = apm_table.cseg_16;
                let cseg_16_len = apm_table.cseg_16_len;
                let dseg = apm_table.dseg;
                let dseg_len = apm_table.dseg_len;
                let entry_offset = apm_table.offset;
                let flags = apm_table.flags;

                eprintln!("Version: {} cs (32-bit): {:#x} len: {:#x} cs (16-bit): {:#x} len: {:#x}",
                          version, cseg, cseg_len, cseg_16, cseg_16_len);
                eprintln!("ds (16-bit): {:#x} len: {:#x} entry offset: {:#x} flags: {:#x}", dseg, dseg_len, entry_offset, flags);
                apm_table
            })
    } else {
        eprint!("NONE.");
        None
    };

    eprint!("Reading VBE table...");

    multiboot_info.vbe_table = if raw_info.flags & RawMultibootInfo::GFX_TAB == RawMultibootInfo::GFX_TAB {
        eprintln!("ok.");
        Some(Box::new(VbeTable {
            control_info: raw_info.vbe_control_info,
            interface_seg: raw_info.vbe_interface_seg,
            interface_off: raw_info.vbe_interface_off,
            interface_len: raw_info.vbe_interface_len,
            mode: raw_info.vbe_mode,
            mode_info: raw_info.vbe_mode_info
        }))
    } else {
        eprintln!("NONE.");
        None
    };

    eprint!("Reading framebuffer table...");

    multiboot_info.frame_buffer_table = if raw_info.flags & RawMultibootInfo::FB_TAB == RawMultibootInfo::FB_TAB {
        eprintln!("ok.");

        match raw_info.framebuffer_type {
            RawFbColorType::INDEXED => {
                  let mut palette: Vec<Color> = Vec::with_capacity(unsafe { raw_info.color_type.indexed.framebuffer_palette_num_colors } as usize);
                  let mut failed = false;

                  for i in 0..unsafe { raw_info.color_type.indexed.framebuffer_palette_num_colors } {
                      //let mut color = Color::default();
                      //let result;

                      if let Some(color) = unsafe { phys::try_from_phys::<Color>((raw_info.color_type.indexed.framebuffer_palette_addr
                          + i as u32 * mem::size_of::<Color>() as u32) as PAddr) } {
                          palette.push(*color)
                      } else {
                          failed = true;
                          break;
                      }
                  }
              if failed {
                  None
              } else {
                  Some(Box::new(FbTable {
                      addr: raw_info.framebuffer_addr,
                      pitch: raw_info.framebuffer_pitch,
                      width: raw_info.framebuffer_width,
                      height: raw_info.framebuffer_height,
                      bpp: raw_info.framebuffer_bpp,
                      color_type: FbColor::Indexed { palette, },
                  }))
              }
          },
            RawFbColorType::RGB => {
              unsafe {
                  Some(Box::new(FbTable {
                      addr: raw_info.framebuffer_addr,
                      pitch: raw_info.framebuffer_pitch,
                      width: raw_info.framebuffer_width,
                      height: raw_info.framebuffer_height,
                      bpp: raw_info.framebuffer_bpp,
                      color_type: FbColor::RGB {
                          red: RgbColor {
                              field_pos: raw_info.color_type.rgb.framebuffer_red_field_position,
                              mask_size: raw_info.color_type.rgb.framebuffer_red_mask_size,
                          },
                          green: RgbColor {
                              field_pos: raw_info.color_type.rgb.framebuffer_green_field_position,
                              mask_size: raw_info.color_type.rgb.framebuffer_green_mask_size,
                          },
                          blue: RgbColor {
                              field_pos: raw_info.color_type.rgb.framebuffer_blue_field_position,
                              mask_size: raw_info.color_type.rgb.framebuffer_blue_mask_size,
                          }
                      },
                  }))
              }
          },
          RawFbColorType::EGA => {
              Some(Box::new(FbTable {
                  addr: raw_info.framebuffer_addr,
                  pitch: raw_info.framebuffer_pitch,
                  width: raw_info.framebuffer_width,
                  height: raw_info.framebuffer_height,
                  bpp: raw_info.framebuffer_bpp,
                  color_type: FbColor::EGA,
              }))
          },
          _ => None,
      }
    } else {
        eprintln!("NONE.");
        None
    };

    Some(multiboot_info)
}

#[derive(Clone, Default)]
pub struct MemorySize {
    pub lower: u64,
    pub upper: u64,
}

#[derive(Clone, Default)]
pub struct BootDevice {
    pub drive: u8,
    pub partition1: u8, // partition
    pub partition2: u8, // sub-partition level 1
    pub partition3: u8, // sub-partition level 2
}

#[derive(Clone, Default)]
pub struct BootModule {
    pub addr: PAddr,
    pub length: PSize,
    pub name: Option<String>,
}

#[derive(Default, Copy, Clone)]
pub struct Color {
    red: u8,
    green: u8,
    blue: u8,
}

#[derive(Clone)]
pub struct FbTable {
    pub addr: PAddr,
    pub pitch: u32,
    pub width: u32,
    pub height: u32,
    pub bpp: u8,
    pub color_type: FbColor
}

#[derive(Clone)]
pub struct MemoryMap {
    pub base_addr: PAddr,
    pub length: PSize,
    pub map_type: MmapType,
}

#[derive(Clone)]
pub enum FbColor {
    Indexed {
        palette: Vec<Color>,
    },
    RGB {
        red: RgbColor,
        green: RgbColor,
        blue: RgbColor,
    },
    EGA,
}

#[derive(Default, Clone)]
pub struct RgbColor {
    pub field_pos: u8,
    pub mask_size: u8,
}

#[derive(Default, Clone)]
pub struct MultibootInfo {
    pub mem_size: Option<MemorySize>,
    pub boot_device: Option<BootDevice>,
    pub command_line: Option<String>,
    pub modules: Option<Vec<BootModule>>,
    pub symbol_table: Option<Box<SymbolTable>>,
    pub section_header: Option<Box<SectionHeaderTable>>,
    pub memory_map: Option<Vec<MemoryMap>>,
    pub drives: Option<Vec<Drive>>,
    pub config_table: Option<PAddr>,
    pub bootloader_name: Option<String>,
    pub apm_table: Option<Box<ApmTable>>,
    pub vbe_table: Option<Box<VbeTable>>,
    pub frame_buffer_table: Option<Box<FbTable>>
}

#[derive(Copy, Clone, Default)]
#[repr(packed(1))]
pub struct ApmTable {
    pub version: u16,
    pub cseg: u16,
    pub offset: u32,
    pub cseg_16: u16,
    pub dseg: u16,
    pub flags: u16,
    pub cseg_len: u16,
    pub cseg_16_len: u16,
    pub dseg_len: u16,
}

#[derive(Copy, Clone, Default)]
#[repr(packed(1))]
pub struct VbeTable {
    pub control_info: u32,
    pub mode_info: u32,
    pub mode: u16,
    pub interface_seg: u16,
    pub interface_off: u16,
    pub interface_len: u16,
}

#[derive(Default, Clone, Copy)]
#[repr(packed(1))]
pub struct RawMultibootInfo {
    pub flags: u32,
    pub mem_lower: u32,
    pub mem_upper: u32,
    pub boot_device: u32,
    pub cmdline: u32,
    pub mods_count: u32,
    pub mods_addr: u32,
    pub syms: RawTables,
    pub mmap_length: u32,
    pub mmap_addr: u32,
    pub drives_length: u32,
    pub drives_addr: u32,
    pub config_table: u32,
    pub boot_loader_name: u32,
    pub apm_table: u32,
    pub vbe_control_info: u32,
    pub vbe_mode_info: u32,
    pub vbe_mode: u16,
    pub vbe_interface_seg: u16,
    pub vbe_interface_off: u16,
    pub vbe_interface_len: u16,
    pub framebuffer_addr: u64,
    pub framebuffer_pitch: u32,
    pub framebuffer_width: u32,
    pub framebuffer_height: u32,
    pub framebuffer_bpp: u8,
    pub framebuffer_type: u8,
    pub color_type: RawFbColorType,
}

impl RawMultibootInfo {
    pub const MEM: u32 = 1u32 << 0;  /* 'mem_*' fields are valid */
    pub const BOOT_DEV: u32 = 1u32 << 1;  /* 'boot_device' field is valid */
    pub const CMDLINE: u32 = 1u32 << 2;  /* 'cmdline' field is valid */
    pub const MODS: u32 = 1u32 << 3;  /* 'mods' fields are valid */
    pub const SYMTAB: u32 = 1u32 << 4;  /* 'syms.symtab' field is valid */
    pub const SHDR: u32 = 1u32 << 5;  /* 'syms.shdr' field is valid */
    pub const MMAP: u32 = 1u32 << 6;  /* 'mmap_*' fields are valid. */
    pub const DRIVES: u32 = 1u32 << 7;  /* 'drives_*' fields are valid */
    pub const CONFIG: u32 = 1u32 << 8;  /* 'config_table' field is valid */
    pub const BOOTLDR: u32 = 1u32 << 9;  /* 'boot_loader_name' field is valid */
    pub const APM_TAB: u32 = 1u32 << 10; /* 'apm_table' field is valid */
    pub const GFX_TAB: u32 = 1u32 << 11; /* Graphics table is available */
    pub const FB_TAB: u32 = 1u32 << 12; /* Framebuffer table is available */
}

#[repr(packed(1))]
#[derive(Copy, Clone)]
pub union RawFbColorType {
    pub indexed: RawIndexedColor,
    pub rgb: RawRgbColor,
}

impl RawFbColorType {
    pub const INDEXED: u8 = 0;
    pub const RGB: u8 = 1;
    pub const EGA: u8 = 2;
}

impl Default for RawFbColorType {

    fn default() -> Self {
        Self {
            indexed: RawIndexedColor::default(),
        }
    }
}

#[repr(packed(1))]
#[derive(Default, Clone, Copy)]
pub struct RawIndexedColor {
    pub framebuffer_palette_addr: u32,
    pub framebuffer_palette_num_colors: u16
}

#[repr(packed(1))]
#[derive(Default, Clone, Copy)]
pub struct RawRgbColor {
    pub framebuffer_red_field_position: u8,
    pub framebuffer_red_mask_size: u8,
    pub framebuffer_green_field_position: u8,
    pub framebuffer_green_mask_size: u8,
    pub framebuffer_blue_field_position: u8,
    pub framebuffer_blue_mask_size: u8,
}

#[repr(packed(1))]
#[derive(Default, Clone, Copy)]
struct RawColorDescriptor {
    pub red: u8,
    pub green: u8,
    pub blue: u8,
}

#[repr(packed(1))]
#[derive(Default, Clone, Copy)]
struct RawModule {
    pub mod_start: u32,
    pub mod_end: u32,
    pub string: u32,
    pub reserved: u32,
}

#[derive(Default, Clone, Copy)]
#[repr(packed(1))]
pub struct RawMemoryMap {
    pub size: u32, // doesn't include itself
    pub base_addr: u64,
    pub length: u64,
    pub map_type: u32,
}

impl RawMemoryMap {
    pub const AVAILABLE: u32 = 1;   // Available RAM
    pub const ACPI: u32 = 3;        // ACPI tables
    pub const RESERVED: u32 = 4;    // Reserved RAM that must be preserved upon hibernation
    pub const BAD: u32 = 5;         // Defective RAM
}

#[derive(Default, Clone, Copy)]
#[repr(packed(1))]
pub struct RawProgramHeader32 {
    pub header_type: u32,
    pub offset: u32,
    pub vaddr: u32,
    pub _resd: u32,
    pub file_size: u32,
    pub mem_size: u32,
    pub flags: u32,
    pub align: u32,
}

impl RawProgramHeader32 {
    pub const NULL: u32 = 0;
    pub const LOAD: u32 = 1;
    pub const DYNAMIC: u32 = 2;
    pub const INTERP: u32 = 3;
    pub const NOTE: u32 = 4;
    pub const SHLIB: u32 = 5;
    pub const PHDR: u32 = 6;
    pub const LOPROC: u32 = 0x70000000;
    pub const HIPROC: u32 = 0x7fffffff;

    pub const EXECUTE: u32 = 1;
    pub const WRITE: u32 = 2;
    pub const READ: u32 = 4;
    pub const MASKPROC: u32 = 0xf0000000;
}

pub fn is_valid_elf_exe(image: PAddr) -> Result<Box<RawElfHeader>, ()> {
    //let mut header = RawElfHeader::default();

    if let Some(header) = unsafe { phys::try_from_phys::<RawElfHeader>(image) } {
        if header.identifier[..4] == RawElfHeader::ELF_MAGIC
            && header.identifier[RawElfHeader::EI_VERSION] == RawElfHeader::EV_CURRENT
            && header.elf_type == RawElfHeader::ET_EXEC
            && header.machine == RawElfHeader::EM_386
            && header.version == RawElfHeader::EV_CURRENT as u32
            && header.identifier[RawElfHeader::EI_CLASS] == RawElfHeader::ELFCLASS32 {
            return Ok(header)
        }
    }

    Err(())
}

pub mod loader {
    use crate::elf::{BootModule, RawElfHeader, RawProgramHeader32};
    use crate::Tid;
    use crate::syscall;
    use crate::syscall::c_types::{NULL_TID, ThreadInfo};
    use core::mem;
    use core::ffi::c_void;
    use crate::pager::page_alloc::PhysPageAllocator;
    use crate::page::{PhysicalPage, VirtualPage};
    use crate::address::{PAddr, VAddr, PSize};
    use crate::mapping::{self, AddrSpace};
    use crate::lowlevel::phys;
    use crate::address;
    use crate::device::{self, DeviceId};
    use crate::syscall::{ThreadStruct, INIT_TID};

    pub fn load_init_mappings(module: &BootModule) -> bool {
        let stack_top = 0xC0000000usize;
        let stack_size = 64*1024usize - 4096usize;
        let tid = Tid::new(INIT_TID);
        let addr_space= mapping::manager::lookup_tid_mut(&tid)
            .expect("Initial address space wasn't found.");

        if module.length as usize >= mem::size_of::<RawElfHeader>() {
            super::is_valid_elf_exe(module.addr)
                .map(|header| {
                    let mut pheader_option;
                    let pmem_device = DeviceId::new_from_tuple((device::mem::MAJOR, device::mem::PMEM_MINOR));
                    let zero_device = DeviceId::new_from_tuple((device::pseudo::MAJOR, device::pseudo::ZERO_MINOR));

                    addr_space.map(Some((stack_top - stack_size) as VAddr),
                                   &zero_device, 0, AddrSpace::EXTEND_DOWN | AddrSpace::NO_EXECUTE, stack_size);
                    addr_space.map(Some((stack_top - stack_size - VirtualPage::SMALL_PAGE_SIZE) as VAddr),
                                   &zero_device, 0, AddrSpace::GUARD | AddrSpace::NO_EXECUTE, VirtualPage::SMALL_PAGE_SIZE);

                    for ph_index in 0..header.phnum {
                        pheader_option = unsafe {
                            phys::try_from_phys::<RawProgramHeader32>((module.addr + header.phoff as PSize + ph_index as PSize * header.phentsize as PSize) as PAddr)
                        };

                        if let Some(pheader) = pheader_option {
                            let flags = if pheader.flags == RawProgramHeader32::READ {
                                AddrSpace::READ_ONLY | AddrSpace::NO_EXECUTE
                            } else if pheader.flags == (RawProgramHeader32::READ | RawProgramHeader32::EXECUTE)
                                || pheader.flags == RawProgramHeader32::EXECUTE {
                                AddrSpace::READ_ONLY
                            } else if pheader.flags == RawProgramHeader32::WRITE
                                || pheader.flags == (RawProgramHeader32::READ | RawProgramHeader32::WRITE) {
                                AddrSpace::NO_EXECUTE
                            } else {
                                0
                            };

                            if pheader.header_type == RawProgramHeader32::LOAD && pheader.flags != 0 {
                                if pheader.file_size > 0 {
                                    addr_space.map(Some(address::u32_into_vaddr(pheader.vaddr)),
                                                   &pmem_device, (module.addr + pheader.offset as PSize) as usize,
                                                   flags, pheader.file_size as usize);
                                }

                                if pheader.mem_size > pheader.file_size {
                                    addr_space.map(Some(address::u32_into_vaddr(pheader.vaddr + pheader.file_size)),
                                                   &zero_device, 0, 0, (pheader.mem_size - pheader.file_size) as usize);
                                }
                            }
                        }
                    }
                }).is_ok()
        } else {
            false
        }
    }

    pub fn load_module(module: &BootModule) -> Result<Tid, ()> {
        let stack_top = 0xC0000000usize;
        let stack_size = 64*1024usize - 4096usize;
        let pmap_option = PhysPageAllocator::alloc()
            .map(|frame| PhysicalPage::from_frame(frame).as_address());
        let pmap;

        if pmap_option.is_none() {
            return Err(());
        } else {
            pmap = pmap_option.unwrap();
        }

        if module.length as usize >= mem::size_of::<RawElfHeader>() {
            super::is_valid_elf_exe(module.addr)
                .map(|header|
                    unsafe {
                        let entry = header.entry;
                        (header, syscall::sys_create_thread(NULL_TID,
                                                   entry as *const c_void,
                                                   &pmap as *const PAddr,
                                                   stack_top as *const c_void))
                    })
                .and_then(|(header, tid)| {
                    if tid == NULL_TID {
                        Err(())
                    } else {
                        let mut addr_space = AddrSpace::new(pmap);
                        let tid = Tid::new(tid);
                        let mut pheader_option;
                        let pmem_device = DeviceId::new_from_tuple((device::mem::MAJOR, device::mem::PMEM_MINOR));
                        let zero_device = DeviceId::new_from_tuple((device::pseudo::MAJOR, device::pseudo::ZERO_MINOR));

                        addr_space.attach_thread(tid.clone());

                        addr_space.map(Some((stack_top - stack_size) as VAddr),
                        &zero_device, 0, AddrSpace::EXTEND_DOWN | AddrSpace::NO_EXECUTE, stack_size);
                        addr_space.map(Some((stack_top - stack_size - VirtualPage::SMALL_PAGE_SIZE) as VAddr),
                                       &zero_device, 0, AddrSpace::GUARD | AddrSpace::NO_EXECUTE, VirtualPage::SMALL_PAGE_SIZE);

                        for ph_index in 0..header.phnum {
                            pheader_option = unsafe {
                                phys::try_from_phys::<RawProgramHeader32>((module.addr + header.phoff as PSize + ph_index as PSize * header.phentsize as PSize) as PAddr)
                            };

                            if let Some(pheader) = pheader_option {
                                let flags = if pheader.flags == RawProgramHeader32::READ {
                                    AddrSpace::READ_ONLY | AddrSpace::NO_EXECUTE
                                } else if pheader.flags == (RawProgramHeader32::READ | RawProgramHeader32::EXECUTE)
                                    || pheader.flags == RawProgramHeader32::EXECUTE {
                                    AddrSpace::READ_ONLY
                                } else if pheader.flags == RawProgramHeader32::WRITE
                                    || pheader.flags == (RawProgramHeader32::READ | RawProgramHeader32::WRITE) {
                                    AddrSpace::NO_EXECUTE
                                } else {
                                    0
                                };

                                if pheader.header_type == RawProgramHeader32::LOAD && pheader.flags != 0 {
                                    if pheader.file_size > 0 {
                                        addr_space.map(Some(address::u32_into_vaddr(pheader.vaddr)),
                                                       &pmem_device, (module.addr + pheader.offset as PSize) as usize,
                                                       flags, pheader.file_size as usize);
                                    }

                                    if pheader.mem_size > pheader.file_size {
                                        addr_space.map(Some(address::u32_into_vaddr(pheader.vaddr + pheader.file_size)),
                                                       &zero_device, 0, 0, (pheader.mem_size - pheader.file_size) as usize);
                                    }
                                }
                            }
                        }

                        let mut thread_info= ThreadInfo::default();
                        thread_info.status = ThreadInfo::READY;

                        let thread_struct = ThreadStruct::new(thread_info, ThreadInfo::STATUS);

                        mapping::manager::register(addr_space);
                        syscall::update_thread(&tid, &thread_struct)
                        .map(|_| tid)
                        .map_err(|_| ())
                        /*Ok(tid)*/
                    }
                })
        } else {
            Err(())
        }
    }
}
