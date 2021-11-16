use crate::address::{PAddr, PSize};
use crate::lowlevel::{phys};
use core::{mem, cmp};
use alloc::vec::Vec;
use alloc::boxed::Box;
use alloc::string::String;

#[repr(C)]
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

#[derive(Default, Clone)]
pub struct MultibootInfo {
    pub mem_size: Option<MemorySize>,
    pub boot_device: Option<BootDevice>,
    pub command_line: Option<String>,
    pub modules: Option<Vec<BootModule>>,
    pub symbol_table: Option<Box<SymbolTable>>,
    pub section_header: Option<Box<SectionHeaderTable>>,
    pub memory_map: Vec<MemoryMap>,
    pub drives: Option<Vec<Drive>>,
    pub config_table: Option<PAddr>,
    pub bootloader_name: Option<String>,
    pub apm_table: Option<Box<ApmTable>>,
    pub vbe_table: Option<Box<VbeTable>>,
    pub frame_buffer_table: Option<Box<FbTable>>
}

impl MultibootInfo {
    /// Reads a Multiboot info struct and all associated tables from physical memory.
    ///
    /// # Safety
    /// Input address must be a physical address that points to a valid multiboot info struct.
    /// All associated table data and pointers must be valid. The kernel and init server must take
    /// care not to overwrite any of this data before this method is called.

    pub unsafe fn from_phys(raw_info_address: PAddr) -> Option<Box<MultibootInfo>> {
        let mut multiboot_info = Box::new(MultibootInfo::default());

        let raw_info = phys::try_from_phys::<RawMultibootInfo>(raw_info_address)?;

        multiboot_info.mem_size = if raw_info.flags & RawMultibootInfo::MEM == RawMultibootInfo::MEM {
            Some(MemorySize {
                lower: raw_info.mem_lower as u64 * 1024,
                upper: raw_info.mem_upper as u64 * 1024
            })
        } else {
            None
        };

        multiboot_info.boot_device = if raw_info.flags & RawMultibootInfo::BOOT_DEV == RawMultibootInfo::BOOT_DEV {
            let drive = (raw_info.boot_device >> 24) as u8;
            let partition1 = ((raw_info.boot_device >> 16) & 0xFF) as u8;
            let partition2 = ((raw_info.boot_device >> 8) & 0xFF) as u8;
            let partition3 = (raw_info.boot_device & 0xFF) as u8;
            //let _dev_name = if drive < 0x80 { "fd" } else { "hd" };

            Some(BootDevice {
                drive,
                partition1,
                partition2,
                partition3,
            })
        } else {
            None
        };

        multiboot_info.command_line = if raw_info.flags & RawMultibootInfo::CMDLINE == RawMultibootInfo::CMDLINE {
            read_c_str(raw_info.cmdline as PAddr)
        } else {
            None
        };

        multiboot_info.modules = if raw_info.flags & RawMultibootInfo::MODS == RawMultibootInfo::MODS {
            let total_mods = raw_info.mods_count;
            eprintln!("{} modules found.", total_mods);

            let mut modules: Vec<BootModule> = Vec::with_capacity(raw_info.mods_count as usize);

            for i in 0..raw_info.mods_count {
                phys::try_from_phys_arr::<RawModule>(raw_info.mods_addr as PAddr, i as PSize)
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

        multiboot_info.memory_map = RawMultibootInfo::raw_mmap_iter(raw_info_address)
            .map(|raw_mmap_iter| raw_mmap_iter.map(|raw_mmap| MemoryMap::from(raw_mmap))
                .collect::<Vec<MemoryMap>>())
            .unwrap_or(vec![]);

        eprint!("Reading drives...");

        multiboot_info.drives = if raw_info.flags & RawMultibootInfo::DRIVES == RawMultibootInfo::DRIVES {
            let mut offset= 0;
            let mut drives_vec = Vec::new();
            eprintln!();

            while offset < raw_info.drives_length {
                if let Some(drive) = phys::try_from_phys::<RawDrive>(raw_info.drives_addr as PAddr + offset as PSize) {
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

                        let result =
                            phys::peek_ptr((raw_info_address + offset as PSize + port_offset) as PAddr,
                                           &mut ports,
                                           cmp::min(mem::size_of_val(&ports),
                                                    drive.size as usize - (offset as usize + port_offset as usize)));

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

            phys::try_from_phys::<ApmTable>(raw_info.apm_table as PAddr)
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
                    let mut palette: Vec<Color> = Vec::with_capacity(raw_info.color_type.indexed.framebuffer_palette_num_colors as usize);
                    let mut failed = false;

                    for i in 0..raw_info.color_type.indexed.framebuffer_palette_num_colors {
                        //let mut color = Color::default();
                        //let result;

                        if let Some(color) = phys::try_from_phys::<Color>((raw_info.color_type.indexed.framebuffer_palette_addr
                            + i as u32 * mem::size_of::<Color>() as u32) as PAddr) {
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
    Preserve,
    Bad,
    Reserved,
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

impl From<RawMemoryMap> for MemoryMap {
    fn from(raw_mmap: RawMemoryMap) -> Self {
        let map_type = match raw_mmap.map_type {
            RawMemoryMap::AVAILABLE => MmapType::Available,
            RawMemoryMap::ACPI => MmapType::Acpi,
            RawMemoryMap::PRESERVE => MmapType::Preserve,
            RawMemoryMap::BAD => MmapType::Bad,
            _ => MmapType::Reserved
        };

        Self {
            base_addr: raw_mmap.base_addr,
            length: raw_mmap.length,
            map_type
        }
    }
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

#[derive(Copy, Clone, Default)]
#[repr(C)]
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
#[repr(C)]
pub struct VbeTable {
    pub control_info: u32,
    pub mode_info: u32,
    pub mode: u16,
    pub interface_seg: u16,
    pub interface_off: u16,
    pub interface_len: u16,
}

#[derive(Default, Clone, Copy)]
#[repr(packed)]
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

    pub unsafe fn raw_mmap_iter<'a>(raw_info_address: PAddr) -> Option<RawMemoryMapIterator> {
        RawMemoryMapIterator::new(raw_info_address)
    }
}

#[repr(packed)]
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

#[repr(packed)]
#[derive(Default, Clone, Copy)]
pub struct RawIndexedColor {
    pub framebuffer_palette_addr: u32,
    pub framebuffer_palette_num_colors: u16
}

#[repr(packed)]
#[derive(Default, Clone, Copy)]
pub struct RawRgbColor {
    pub framebuffer_red_field_position: u8,
    pub framebuffer_red_mask_size: u8,
    pub framebuffer_green_field_position: u8,
    pub framebuffer_green_mask_size: u8,
    pub framebuffer_blue_field_position: u8,
    pub framebuffer_blue_mask_size: u8,
}

#[repr(packed)]
#[derive(Default, Clone, Copy)]
struct RawColorDescriptor {
    pub red: u8,
    pub green: u8,
    pub blue: u8,
}

#[repr(C)]
#[derive(Default, Clone, Copy)]
struct RawModule {
    pub mod_start: u32,
    pub mod_end: u32,
    pub string: u32,
    pub reserved: u32,
}

#[derive(Default, Clone, Copy)]
#[repr(packed(4))]
pub struct RawMemoryMap {
    pub size: u32, // doesn't include itself
    pub base_addr: u64,
    pub length: u64,
    pub map_type: u32,
}

impl RawMemoryMap {
    pub const AVAILABLE: u32 = 1;   // Available RAM
    pub const ACPI: u32 = 3;        // ACPI tables
    pub const PRESERVE: u32 = 4;    // Reserved RAM that must be preserved upon hibernation
    pub const BAD: u32 = 5;         // Defective RAM

    /// Reads a Multiboot mmap struct directly from physical memory.
    ///
    /// # Safety
    /// Input address must be a physical address that points to a valid Multiboot mmap structure
    /// at least 24 bytes in length.

    pub unsafe fn read_raw(address: PAddr) -> Option<RawMemoryMap> {
        let mut raw_mmap = RawMemoryMap::default();

        if phys::try_read_from_phys(address,&mut raw_mmap) == true {
            Some(raw_mmap)
        } else {
            None
        }
    }
}

impl From<MemoryMap> for RawMemoryMap {
    fn from(mmap: MemoryMap) -> Self {
        let map_type = match mmap.map_type {
            MmapType::Available => RawMemoryMap::AVAILABLE,
            MmapType::Acpi => RawMemoryMap::ACPI,
            MmapType::Preserve => RawMemoryMap::PRESERVE,
            MmapType::Bad => RawMemoryMap::BAD,
            MmapType::Reserved => 0,
        };

        Self {
            size: mem::size_of::<Self>() as u32,
            base_addr: mmap.base_addr,
            length: mmap.length,
            map_type
        }
    }
}

use alloc::rc::Rc;

pub struct RawMemoryMapIterator {
    addr: PAddr,
    length: PSize,
    bytes_read: PSize,
    buffer: Rc<Vec<u8>>,
}

impl RawMemoryMapIterator {
    pub fn new(address: PAddr) -> Option<RawMemoryMapIterator> {
        let mut raw_info = RawMultibootInfo::default();

        let success = unsafe {
            phys::try_read_from_phys(address, &mut raw_info)
        };

        if success && raw_info.flags & RawMultibootInfo::MMAP == RawMultibootInfo::MMAP {
            let mut buffer = Vec::with_capacity(raw_info.mmap_length as usize);
            unsafe {
                phys::peek_ptr(raw_info.mmap_addr as PAddr, buffer.as_mut_ptr(), raw_info.mmap_length as usize)
                    .map(|_|
                        RawMemoryMapIterator {
                            addr: raw_info.mmap_addr as PAddr,
                            length: raw_info.mmap_length as PSize,
                            bytes_read: 0,
                            buffer: Rc::new(buffer),
                        })
                    .ok()
            }
        } else {
            None
        }
    }

    pub fn reset(&mut self) {
        self.bytes_read = 0;
    }
}

impl Clone for RawMemoryMapIterator {
    fn clone(&self) -> Self {
        Self {
            addr: self.addr,
            length: self.length,
            bytes_read: self.bytes_read,
            buffer: self.buffer.clone()
        }
    }
}

impl Iterator for RawMemoryMapIterator {
    type Item = RawMemoryMap;

    fn next(&mut self) -> Option<Self::Item> {
        if self.length - self.bytes_read < mem::size_of::<RawMemoryMap>() as PSize {
            self.bytes_read = self.length;
            None
        } else {
            let raw_mmap = unsafe {
                (self.buffer
                    .as_ptr()
                    .wrapping_add(self.bytes_read as usize) as *const RawMemoryMap)
                    .read()
            };

            self.bytes_read += raw_mmap.size as PSize + mem::size_of_val(&raw_mmap.size) as PSize;

            Some(raw_mmap)
        }
    }
}