use crate::address::{PAddr};
use crate::lowlevel::phys;
use alloc::boxed::Box;

#[repr(C)]
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
    use crate::elf::{RawElfHeader, RawProgramHeader32};
    use rust::types::Tid;
    use rust::syscalls;
    use rust::syscalls::c_types::NULL_TID;
    use core::mem;
    use core::ffi::c_void;
    use crate::page::VirtualPage;
    use crate::address::{PAddr, VAddr, PSize};
    use crate::mapping::{self, AddrSpace};
    use crate::lowlevel::phys;
    use crate::address;
    use crate::device::{self, DeviceId};
    use rust::syscalls::{ThreadInfo, INIT_TID};
    use crate::multiboot::BootModule;
    use crate::phys_alloc::{self, BlockSize, AllocError};

    pub fn load_init_mappings(module: &BootModule) -> bool {
        let stack_top = 0xC0000000usize;
        let stack_size = 64*1024usize - 4096usize;
        let tid = Tid::new(INIT_TID).expect("Init tid shouldn't be null.");
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
                                                   &pmem_device, module.addr + pheader.offset as u64,
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
        let stack_size = 4096*1024usize;
        let pmap = phys_alloc::alloc_phys(BlockSize::Block4k)
            .map(|(addr, _)| addr)
            .map_err(|_| ())?;

        if module.length as usize >= mem::size_of::<RawElfHeader>() {
            super::is_valid_elf_exe(module.addr)
                .map(|header|
                    unsafe {
                        let entry = header.entry;
                        (header, syscalls::sys_create_thread(
                                                   entry as *const c_void,
                                                   pmap as u32,
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

                        addr_space.map(Some((stack_top - stack_size + VirtualPage::SMALL_PAGE_SIZE) as VAddr),
                                       &zero_device, 0, AddrSpace::EXTEND_DOWN | AddrSpace::NO_EXECUTE, stack_size - VirtualPage::SMALL_PAGE_SIZE);
                        addr_space.map(Some((stack_top - stack_size) as VAddr),
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
                                                       &pmem_device, (module.addr + pheader.offset as PSize) as u64,
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
                        syscalls::update_thread(&tid, &thread_struct)
                        .map(|_| tid)
                        .map_err(|_| { println!("Unable to update thread"); () })
                        /*Ok(tid)*/
                    }
                })
        } else {
            Err(())
        }
    }
}
