#![allow(dead_code)]

use c_types::{CTid, ThreadInfo};
use crate::message::{RawMessage};
use crate::error::Error;
use crate::address::{PAddr};
use crate::Tid;
use crate::page::{PhysicalPage};
use crate::error;
use core::ffi::c_void;
use core::cmp::min;

pub const INIT_TID: CTid = 1024;
pub const IRQ0_TID: CTid = 1;

pub mod c_types {
    pub type CAddr = u32;
    pub type CPhysAddr = u64;
    pub type CTid = u16;

    pub const NULL_TID: CTid = 0;
    use super::flags::thread;
    use core::ffi::c_void;
    use core::ptr;

    #[repr(C)]
    pub struct RegisterState {
        pub eax: u32,
        pub ebx: u32,
        pub ecx: u32,
        pub edx: u32,
        pub ebp: u32,
        pub esp: u32,
        pub esi: u32,
        pub edi: u32,
        pub eip: u32,
        pub eflags: u32,
        pub cs: u16,
        pub ds: u16,
        pub es: u16,
        pub fs: u16,
        pub gs: u16,
        pub ss: u16,
    }

    #[repr(C)]
    pub struct ThreadInfo {
        pub priority: u32,
        pub status: u32,
        pub root_page_map: CPhysAddr,
        pub wait_tid: CTid,
        pub state: RegisterState,
        pub ext_reg_state: * mut c_void,
    }

    impl ThreadInfo {
        pub const STATUS: u32 = thread::STATUS;
        pub const PRIORITY: u32 = thread::PRIORITY;
        pub const REG_STATE: u32 = thread::REG_STATE;
        pub const PMAP: u32 = thread::PMAP;
        pub const EXT_REG_STATE: u32 = thread::EXT_REG_STATE;

        pub const INACTIVE: u32 = 0;
        pub const PAUSED: u32 = 1;
        pub const SLEEPING: u32 = 2;
        pub const READY: u32 = 3;
        pub const RUNNING: u32 = 4;
        pub const WAIT_FOR_SEND: u32 = 5;
        pub const WAIT_FOR_RECV: u32 = 6;
        pub const ZOMBIE: u32 = 7;
        pub const IRQ_WAIT: u32 = 8;
    }

    impl Default for ThreadInfo {
        fn default() -> ThreadInfo {
            ThreadInfo {
                priority: 0,
                status: 0,
                root_page_map: 0,
                wait_tid: 0,
                state: RegisterState::default(),
                ext_reg_state: ptr::null_mut(),
            }
        }
    }

    impl Default for RegisterState {
        fn default() -> RegisterState {
            RegisterState {
                eax: 0,
                ebx: 0,
                ecx: 0,
                edx: 0,
                ebp: 0,
                esp: 0,
                esi: 0,
                edi: 0,
                eip: 0,
                eflags: 0,
                cs: 0,
                ss: 0
            }
        }
    }
}

pub struct ThreadStruct {
    info: ThreadInfo,
    flags: u32,
}

impl ThreadStruct {
    pub fn new(info: ThreadInfo, flags: u32) -> Self {
        Self { info, flags: flags as u32 }
    }

    pub fn priority(&self) -> Option<&u32> {
        if self.flags & ThreadInfo::PRIORITY == 0 {
            None
        } else {
            Some(&self.info.priority)
        }
    }

    pub fn status(&self) -> Option<&u32> {
        if self.flags & ThreadInfo::STATUS == 0 {
            None
        } else {
            Some(&self.info.status)
        }
    }

    pub fn root_pmap(&self) -> Option<PhysicalPage> {
        if self.flags & ThreadInfo::PMAP == 0 {
            None
        } else {
            Some(PhysicalPage::new(self.info.root_page_map as PAddr))
        }
    }

    pub fn wait_tid(&self) -> Option<Tid> {
        if self.flags & ThreadInfo::STATUS == 0 {
            None
        } else {
            Some(Tid::from(self.info.wait_tid))
        }
    }

    pub fn state(&self) -> Option<&c_types::RegisterState> {
        if self.flags & ThreadInfo::REG_STATE == 0 {
            None
        } else {
            Some(&self.info.state)
        }
    }
}

pub mod status {
    pub const OK: i32 = 0;
    pub const ARG: i32 = -1;
    pub const FAIL: i32 = -2;
    pub const PERM: i32 = -3;
    pub const BADCALL: i32 = -4;
    pub const NOTIMPL: i32 = -5;
    pub const NOTREADY: i32 = -6;
}

pub mod flags {
    pub mod map {
        pub const PRESENT: u32 = 0x01;
        pub const NOT_PRESENT: u32 = 0;
        pub const READ_ONLY: u32 = 0;
        pub const READ_WRITE: u32 = 0x02;
        pub const PM_WRITE_THROUGH: u32 = 0x08;
        pub const PM_NOT_CACHED: u32 = 0x10;
        pub const PM_ACCESSED: u32 = 0x20;
        pub const PM_DIRTY: u32 = 0x40;
        pub const PM_LARGE_PAGE: u32 = 0x80;
        pub const OVERWRITE: u32 = 0x20000000;  // if set to 1, then sys_map() will not fail
                                                // if an address maps to a previous entry that's already
                                                // marked as present.

        pub const ARRAY: u32 = 0x40000000;      // if set to 1, then an array of physical frames
                                                // is passed as input. Otherwise a pointer to a
                                                // starting physical frame will be used.
        pub const INVALIDATE: u32 = 0x80000000;

        pub const PAGE_FLAG_MASK: u64 = 0xFFF;
        pub const LARGE_PAGE_FLAG_MASK: u64 = 0x1FFF;
    }

    pub mod unmap {
        pub const WAS_LARGE_PAGE: u64 = 0x01;   // indicates whether the unmapped pages was 4 kB (0) or 4 MB (1)
        pub const READ_ONLY: u64 = 0x00;
        pub const READ_WRITE: u64 = 0x02;
        pub const KERNEL: u64 = 0x00;
        pub const USER: u64 = 0x04;
        pub const WRITE_THROUGH: u64 = 0x08;
        pub const NOT_CACHED: u64 = 0x10;
        pub const ACCESSED: u64 = 0x20;
        pub const DIRTY: u64 = 0x40;
        pub const LARGE_PAGE: u64 = 0x80;
        pub const PTE_PAT: u64 = 0x80;      // only valid if WAS_LARGE_FLAG bit = 0
        pub const GLOBAL: u64 = 0x100;
        pub const PDE_PAT: u64 = 0x1000;   // only valid if WAS_LARGE_FLAG bit = 1

        pub const PAGE_FLAG_MASK: u64 = 0xFFF;
        pub const LARGE_PAGE_FLAG_MASK: u64 = 0x1FFF;
    }

    pub mod thread {
        pub const STATUS: u32 = 1;
        pub const PRIORITY: u32 = 2;
        pub const REG_STATE: u32 = 4;
        pub const PMAP: u32 = 8;
        pub const EXT_REG_STATE: u32 = 16;
    }
}

pub fn exit(code: i32) -> ! {
    unsafe { sys_exit(code) }
}

pub fn sleep(timeout: u32) -> Result<(), Error> {
    let result = unsafe { sys_sleep(timeout) };

    match result {
        status::OK => Ok(()),
        err => Err(err)
    }
}

pub unsafe fn map(root_map: Option<PAddr>, vaddr: *mut c_void, paddr: PAddr, num_pages: i32, flags: u32)
                         -> Result<i32, Error> {
    let result = {
        let pmap = match root_map {
            Some(a) => &a as *const PAddr,
            None => core::ptr::null()
        };

        let frame = [PhysicalPage::new(paddr)];

        sys_map(pmap,
                vaddr,
                frame.as_ptr() as *const PAddr,
                num_pages,
                flags & !self::flags::map::ARRAY)
    };

    if result >= 0 {
        Ok(result)
    } else {
        Err(result)
    }
}

pub unsafe fn map_frames(root_map: Option<PAddr>, vaddr: *mut c_void, page_frames: &[PAddr], num_pages: i32, flags: u32)
                         -> Result<i32, Error> {
    let result = {
        let pmap = match root_map {
            Some(a) => &a as *const PAddr,
            None => core::ptr::null()
        };

        if page_frames.len() == 1 {
            sys_map(pmap,
                    vaddr,
                    &page_frames[0] as *const PAddr,
                    num_pages,
                    flags & !self::flags::map::ARRAY)
        }
        else {
            sys_map(pmap,
            vaddr,
            page_frames.as_ptr() as *const PAddr,
            min(num_pages, page_frames.len() as i32),
            flags | self::flags::map::ARRAY)
        }
    };

    if result >= 0 {
        Ok(result)
    } else {
        Err(result)
    }
}

pub unsafe fn unmap(root_map: Option<PAddr>, vaddr: *const c_void, num_pages: i32, mut unmapped_frames: Option<(&mut [u64], &mut [u64])>) -> Result<i32, Error> {
    let result = {
        let r;

        {
            let pmap = match root_map {
                Some(a) => &a as *const PAddr,
                None => core::ptr::null()
            };

            let unmapped = match &unmapped_frames {
                Some((frames, entries)) => {
                    if frames.len() != entries.len() {
                        return Err(error::BAD_ARGUMENT)
                    }

                    (*frames).as_ptr() as *mut PAddr
                },
                None => core::ptr::null_mut()
            };

            r = sys_unmap(pmap, vaddr, num_pages, unmapped)
        }

        if let Some((uframes, uentries)) = &mut unmapped_frames {
            for i in 0..uframes.len() {
                let is_large_page = uframes[i] & self::flags::unmap::WAS_LARGE_PAGE != 0;

                uentries[i] = uframes[i] & if is_large_page { !self::flags::unmap::LARGE_PAGE_FLAG_MASK } else { !self::flags::unmap::PAGE_FLAG_MASK };
                uframes[i] &= if is_large_page { !self::flags::unmap::LARGE_PAGE_FLAG_MASK } else { !self::flags::unmap::PAGE_FLAG_MASK };
            }
        }

        r
    };

    if result >= 0 {
        Ok(result)
    } else {
        Err(result)
    }
}

pub fn read_thread(tid: &Tid, flags: u32) -> Result<ThreadStruct, Error> {
    let mut info = ThreadInfo::default();

    let result = unsafe {
        sys_read_thread(tid.into(), flags, &mut info)
    };

    match result {
        status::OK => Ok(ThreadStruct::new(info, flags)),
        err => Err(err),
    }
}

pub fn update_thread(tid: &Tid, thread_struct: &ThreadStruct) -> Result<(), Error> {
    let result = unsafe {
        sys_update_thread(tid.into(), thread_struct.flags.clone(), &thread_struct.info)
    };

    match result {
        status::OK => Ok(()),
        err => Err(err),
    }
}

pub fn get_init_pmap() -> Result<PhysicalPage, Error> {
    read_thread(&Tid::from(INIT_TID), ThreadInfo::PMAP)
        .and_then(|thread_struct| thread_struct
            .root_pmap()
            .ok_or_else(|| error::OPERATION_FAILED))
}

#[link(name="os_init", kind="static")]
extern "C" {
    /*
    pub fn init_kernel(tid: u16) -> i32;
    pub fn cleanup_kernel();
*/
    pub fn sys_send(in_msg: *mut RawMessage) -> i32;
    pub fn sys_call(in_msg: *mut RawMessage, out_msg: *mut RawMessage) -> i32;
    pub fn sys_receive(out_msg: *mut RawMessage) -> i32;
    pub fn sys_exit(code: i32) -> !;
    pub fn sys_sleep(timeout: u32) -> i32;
    pub fn sys_map(root_map: * const PAddr, vaddr: * const c_void,
                   page_frames: * const PAddr, num_pages: i32,
                   flags: u32) -> i32;
    pub fn sys_unmap(root_map: * const PAddr, vaddr: * const c_void,
                     num_pages: i32, unmapped_frames: * mut PAddr) -> i32;
    pub fn sys_create_thread(tid: CTid, entry: * const c_void, root_pmap: * const PAddr,
                         stack_top: * const c_void) -> CTid;
    pub fn sys_destroy_thread(tid: CTid) -> i32;
    pub fn sys_read_thread(tid: CTid, flags: u32, info: *mut ThreadInfo) -> i32;
    pub fn sys_update_thread(tid: CTid, flags: u32, info: *const ThreadInfo) -> i32;
}
