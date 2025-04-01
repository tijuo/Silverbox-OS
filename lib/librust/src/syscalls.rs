pub use crate::types::Tid;
pub use self::c_types::{CTid, CPageMap};
use core::result;
pub use core::ffi::{c_void, c_int, c_uint};
use crate::syscalls::c_types::{CURRENT_ROOT_PMAP, NULL_TID};
use crate::message::MessageHeader;
use core::arch::asm;
use crate::types::PAddr;

macro_rules! syscall_with_number {
    ($sc_num:expr, $arg1:expr) => {
        {
            let mut return_value: i32;
            let mut return_args: [u32; 3] = [0; 3];
            unsafe {
                asm!(
                    "xchg edx, esi",
                    "push ebp",
                    "push edx",
                    "push ecx",
                    "lea edx, 2f",
                    "mov ebp, esp",
                    "sysenter",
                    "2:",
                    "mov esp, ebp",
                    "pop ecx",
                    "pop edx",
                    "pop ebp",
                    "xchg edx, esi",
                    inout("eax") ($sc_num) as u32 => return_value,
                    inout("ebx") ($arg1) as u32 => return_args[0],
                    out("edx") return_args[1],
                    out("edi") return_args[2]
                );
            }
            (return_value, return_args[0], return_args[1], return_args[2])
        }
    };
    ($sc_num:expr, $arg1:expr, $arg2:expr) => {
        {
            let mut return_value: i32;
            let mut return_args: [u32; 3] = [0; 3];
            unsafe {
                asm!(
                    "xchg edx, esi",
                    "push ebp",
                    "push edx",
                    "push ecx",
                    "lea edx, 2f",
                    "mov ebp, esp",
                    "sysenter",
                    "2:",
                    "mov esp, ebp",
                    "pop ecx",
                    "pop edx",
                    "pop ebp",
                    "xchg edx, esi",
                    inout("eax") ($sc_num) as u32 => return_value,
                    inout("ebx") ($arg1) as u32 => return_args[0],
                    in("ecx") ($arg2) as u32,
                    out("edx") return_args[1],
                    out("edi") return_args[2],
                );
            }
            (return_value, return_args[0], return_args[1], return_args[2])
        }
    };
    ($sc_num:expr, $arg1:expr, $arg2:expr, $arg3:expr) => {
        {
            let mut return_value: i32;
            let mut return_args: [u32; 3] = [0; 3];
            unsafe {
                asm!(
                    "xchg edx, esi",
                    "push ebp",
                    "push edx",
                    "push ecx",
                    "lea edx, 2f",
                    "mov ebp, esp",
                    "sysenter",
                    "2:",
                    "mov esp, ebp",
                    "pop ecx",
                    "pop edx",
                    "pop ebp",
                    "xchg edx, esi",
                    inout("eax") ($sc_num) as u32 => return_value,
                    inout("ebx") ($arg1) as u32 => return_args[0],
                    in("ecx") ($arg2) as u32,
                    inout("edx") ($arg3) as u32 => return_args[1],
                    out("edi") return_args[2],
                );
            }
            (return_value, return_args[0], return_args[1], return_args[2])
        }
    };
    ($sc_num:expr, $arg1:expr, $arg2:expr, $arg3:expr, $arg4:expr) => {
        {
            let mut return_value: i32;
            let mut return_args: [u32; 3] = [0; 3];
            unsafe {
                asm!(
                    "xchg edx, esi",
                    "push ebp",
                    "push edx",
                    "push ecx",
                    "lea edx, 2f",
                    "mov ebp, esp",
                    "sysenter",
                    "2:",
                    "mov esp, ebp",
                    "pop ecx",
                    "pop edx",
                    "pop ebp",
                    "xchg edx, esi",
                    inout("eax") ($sc_num) as u32 => return_value,
                    inout("ebx") ($arg1) as u32 => return_args[0],
                    in("ecx") ($arg2) as u32,
                    inout("edx") ($arg3) as u32 => return_args[1],
                    inout("edi") ($arg4) as u32 => return_args[2]
                );
            }
            (return_value, return_args[0], return_args[1], return_args[2])
        }
    };
    ($sc_num:expr) => {
        {
            let mut return_value: i32;
            let mut return_args: [u32; 3] = [0; 3];
            unsafe {
                asm!(
                    "xchg edx, esi",
                    "push ebp",
                    "push edx",
                    "push ecx",
                    "lea edx, 2f",
                    "mov ebp, esp",
                    "sysenter",
                    "2:",
                    "mov esp, ebp",
                    "pop ecx",
                    "pop edx",
                    "pop ebp",
                    "xchg edx, esi",
                    inout("eax") ($sc_num) as u32 => return_value,
                    out("ebx") return_args[0],
                    out("edx") return_args[1],
                    out("edi") return_args[2]
                );
            }
            (return_value, return_args[0], return_args[1], return_args[2])
        }
    };
}

/// Perform a system call to the kernel.
/// 
/// The first argument in the macro should be the system call number followed by up to four
/// optional arguments.
/// 
/// # Example usage:
/// ```
/// let status = syscall!(6, 123, &var);
/// ```
macro_rules! syscall {
    ($sc_num:expr, $($tail:tt)*) => {
        {
           syscall_with_output!($sc_num, $($tail)*).0
        }
    }
}

/// Perform a system call to the kernel.
/// 
/// The first argument in the macro should be the system call number followed by up to four
/// optional arguments. Unlike `syscall!()`, this macro returns all of the system call output
/// values returned in registers.
/// 
/// # Example usage:
/// ```
/// let status = syscall!(6, 123, &var);
/// ```
macro_rules! syscall_with_output {
    ($sc_num:expr, $($tail:tt)*) => {
        {
           syscall_with_number!($sc_num, $($tail)*)
        }
    }
}

pub enum SyscallFunction {
    Create,
    Read,
    Update,
    Destroy,
    Send,
    Sleep,
    Receive
}

pub type Result<T> = result::Result<T, SyscallError>;

#[derive(Debug, Copy, Clone)]
pub enum SyscallError {
    InvalidArgument,
    Failed,
    Unauthorized,
    InvalidSystemCall,
    NotImplemented,
    NotReady,
    Interrupted,
    Preempted,
    PartiallyMapped(usize),
    UnspecifiedError
}

/// Converts a system call return value into a `Result<i32>`
fn syscall_result(value: c_int) -> Result<i32> {
    match value {
        x if x >= 0 => Ok(x as i32),
        status::ARG => Err(SyscallError::InvalidArgument),
        status::FAIL => Err(SyscallError::Failed),
        status::PERM => Err(SyscallError::Unauthorized),
        status::BADCALL => Err(SyscallError::InvalidSystemCall),
        status::NOTIMPL => Err(SyscallError::NotImplemented),
        status::NOTREADY => Err(SyscallError::NotReady),
        status::INT => Err(SyscallError::Interrupted),
        status::PREEMPT => Err(SyscallError::Preempted),
        _ => Err(SyscallError::UnspecifiedError),
    }
}

pub mod c_types {
    use core::ffi::{c_ushort, c_ulong, c_uint, c_int, c_void, c_size_t};
    pub type CAddr = c_ulong;
    pub type CPAddr = c_ulong;
    pub type CTid = c_ushort;
    pub type CPageMap = c_ulong;

    pub const NULL_TID: CTid = 0;
    pub const CURRENT_ROOT_PMAP: CPageMap = 0xFFFFFFFF;

    #[repr(C)]
    #[derive(Debug, Copy, Clone, Default)]
    pub struct MmxRegister {
        pub value: u64,
        pub _resd: u64,
    }

    #[repr(C)]
    #[derive(Debug, Copy, Clone, Default)]
    pub struct FpuRegister {
        pub value: [u8; 10],
        pub _resd: [u8; 6],
    }

    #[repr(C)]
    #[derive(Copy, Clone)]
    pub union MmxFpuRegister {
        pub mm: MmxRegister,
        pub st: FpuRegister,
    }

    #[repr(C)]
    #[derive(Debug, Copy, Clone, Default)]
    pub struct XmmRegister {
        pub low: u64,
        pub high: u64,
    }

    #[repr(C)]
    #[derive(Debug, Copy, Clone)]
    pub(crate) struct ReadPageMappingArgs {
        pub virt: *const c_void,
        pub count: c_size_t,
        pub addr_space: CPAddr,
        pub mappings: *mut PageMapping,
        pub level: c_int
    }

    #[repr(C)]
    #[derive(Debug, Copy, Clone)]
    pub(crate) struct UpdatePageMappingArgs {
        pub virt: *const c_void,
        pub count: c_size_t,
        pub addr_space: CPAddr,
        pub mappings: *const PageMapping,
        pub level: c_int
    }
    
    #[repr(C)]
    #[derive(Debug, Copy, Clone)]
    pub(crate) struct CreateTcbArgs {
        pub entry: *const c_void,
        pub addr_space: CPAddr,
        pub stack_top: *const c_void,
    }
    
    #[repr(C)]
    #[derive(Debug, Copy, Clone)]
    pub(crate) struct ReadTcbArgs {
        pub tid: CTid,
        pub info: *mut ThreadState,
        pub info_length: c_size_t
    }
    
    #[repr(C)]
    #[derive(Debug, Copy, Clone)]
    pub(crate) struct UpdateTcbArgs {
        pub tid: CTid,
        pub flags: c_uint,
        pub info: *const ThreadState,
    }
    
    #[repr(C)]
    #[derive(Debug, Copy, Clone, Default)]
    pub(crate) struct DestroyTcbArgs {
        pub tid: CTid,
    }
    
    #[repr(C)]
    #[derive(Debug, Copy, Clone, Default)]
    pub(crate) struct CreateIntArgs {
        pub irq: c_uint,   // Event interrupts can't be created.
    }
    
    #[repr(C)]
    #[derive(Debug, Copy, Clone, Default)]
    pub(crate) struct ReadIntArgs {
        pub int_mask: c_uint,  // Includes event interrupts.
        pub blocking: c_int,
    }
    
    #[repr(C)]
    #[derive(Debug, Copy, Clone, Default)]
    pub(crate) struct UpdateIntArgs {
        pub int_mask: c_uint,  // Includes event interrupts.
    }
    
    #[repr(C)]
    #[derive(Debug, Copy, Clone, Default)]
    pub(crate) struct DestroyIntArgs {
        pub irq: c_uint,    // Event interrupts can't be destroyed.
    }

    #[repr(C)]
    #[derive(Clone)]
    pub struct LegacyXSaveState {
        pub fcw: u16,
        pub fsw: u16,
        pub ftw: u8,
        pub _resd1: u8,
        pub fop: u16,
        pub fpu_ip: u32,
        pub fpu_cs: u16,
        pub _resd2: u16,
        pub fpu_dp: u32,
        pub fpu_ds: u16,
        pub _resd3: u16,
        pub mxcsr: u32,
        pub mxcsr_mask: u32,
    
        pub mmx_st: [MmxFpuRegister; 8],
        pub xmm: [XmmRegister; 8],
    }
    
    impl Default for LegacyXSaveState {
        fn default() -> Self {
            LegacyXSaveState {
                fcw: 0,
                fsw: 0,
                ftw: 0,
                _resd1: 0,
                fop: 0,
                fpu_ip: 0,
                fpu_cs: 0,
                _resd2: 0,
                fpu_dp: 0,
                fpu_ds: 0,
                _resd3: 0,
                mxcsr: 0,
                mxcsr_mask: 0,
                mmx_st: [MmxFpuRegister { mm: MmxRegister::default() }; 8],
                xmm: [XmmRegister::default(); 8]
            }
        }
    }
    
    #[repr(C)]
    #[derive(Debug, Clone, Default)]
    pub struct RegisterState {
        pub eax: u32,
        pub ebx: u32,
        pub ecx: u32,
        pub edx: u32,
        pub esi: u32,
        pub edi: u32,
        pub ebp: u32,
        pub esp: u32,
        pub cs: u16,
        pub ds: u16,
        pub es: u16,
        pub fs: u16,
        pub gs: u16,
        pub ss: u16,
        pub eflags: u32,
    }
    
    #[repr(C)]
    #[derive(Clone)]
    pub struct ThreadState {
        pub register_state: RegisterState,
        pub root_page_map: CPageMap,
        pub thread_state: u8,
        pub current_processor_id: u8,
        pub tid: CTid,
    
        pub priority: u8,
    
        pub pending_events: u32,
        pub event_mask: u32,
    
        pub cap_table: *const u8,
        pub cap_table_len: usize,
    
        pub exception_handler: CTid,
        pub pager: CTid,
    
        pub wait_tid: CTid,
    
        pub receiver_wait_head: CTid,
        pub receiver_wait_tail: CTid,
    
        pub sender_wait_head: CTid,
        pub sender_wait_tail: CTid,
    
        pub fxsave_state: *mut LegacyXSaveState,
        pub fxsave_state_len: usize,
        pub xsave_rfbm: u16
    }
    
    impl Default for ThreadState {
        fn default() -> Self {
            ThreadState {
                register_state: Default::default(),
                root_page_map: 0,
                thread_state: 0,
                current_processor_id: 0,
                tid: NULL_TID,
                priority: 0,
                pending_events: 0,
                event_mask: 0,
                cap_table: core::ptr::null(),
                cap_table_len: 0,
                exception_handler: 0,
                pager: 0,
                wait_tid: 0,
                receiver_wait_head: 0,
                receiver_wait_tail: 0,
                sender_wait_head: 0,
                sender_wait_tail: 0,
                fxsave_state: core::ptr::null_mut(),
                fxsave_state_len: 0,
                xsave_rfbm: 0
            }
        }
    }
    
    impl ThreadState {
        pub const INACTIVE: u32 = 0;
        pub const PAUSED: u32 = 1;
        pub const ZOMBIE: u32 = 2;
        pub const READY: u32 = 3;
        pub const RUNNING: u32 = 4;
        pub const WAIT_FOR_SEND: u32 = 5;
        pub const WAIT_FOR_RECV: u32 = 6;
        pub const SLEEPING: u32 = 7;
    
        pub const STATUS: u32 = 1;
        pub const PRIORITY: u32 = 2;
        pub const REG_STATE: u32 = 4;
        pub const ROOT_PMAP: u32 = 8;
        pub const EXT_REG_STATE: u32 = 16;
        pub const CAP_TABLE: u32 = 32;
        pub const EX_HANDLER: u32 = 64;
        pub const EVENTS: u32 = 128;
    }
    
    #[repr(C)]
    #[derive(Debug, Default, Copy, Clone)]
    pub struct PageMapping {
        pub number: u32,
        pub flags: u32,
    }
}

pub mod events {
    pub const STOP_CHILD: u32 = 1 << 0;
    pub const STOP_PARENT: u32 = 1 << 1;
    pub const STOP_EH: u32 = 1 << 2;
    pub const STOP_PAGER: u32 = 1 << 3;
    pub const IRQ0: u32 = 1 << 8;
    pub const IRQ1: u32 = 1 << 9;
    pub const IRQ2: u32 = 1 << 10;
    pub const IRQ3: u32 = 1 << 11;
    pub const IRQ4: u32 = 1 << 12;
    pub const IRQ5: u32 = 1 << 13;
    pub const IRQ6: u32 = 1 << 14;
    pub const IRQ7: u32 = 1 << 15;
    pub const IRQ8: u32 = 1 << 16;
    pub const IRQ9: u32 = 1 << 17;
    pub const IRQ10: u32 = 1 << 18;
    pub const IRQ11: u32 = 1 << 19;
    pub const IRQ12: u32 = 1 << 20;
    pub const IRQ13: u32 = 1 << 21;
    pub const IRQ14: u32 = 1 << 22;
    pub const IRQ15: u32 = 1 << 23;
    pub const IRQ16: u32 = 1 << 24;
    pub const IRQ17: u32 = 1 << 25;
    pub const IRQ18: u32 = 1 << 26;
    pub const IRQ19: u32 = 1 << 27;
    pub const IRQ20: u32 = 1 << 28;
    pub const IRQ21: u32 = 1 << 29;
    pub const IRQ22: u32 = 1 << 30;
    pub const IRQ23: u32 = 1 << 31;
}

pub mod status {
    use core::ffi::c_int;

    pub const OK: c_int = 0;
    pub const ARG: c_int = -1;
    pub const FAIL: c_int = -2;
    pub const PERM: c_int = -3;
    pub const BADCALL: c_int = -4;
    pub const NOTIMPL: c_int = -5;
    pub const NOTREADY: c_int = -6;
    pub const INT: c_int = -7;
    pub const PREEMPT: c_int = -8;
}

pub mod flags {
    pub mod mapping {
        /// If set, the page mapping is not valid and will be ignored by the processor.
        /// If cleared, the page mapping is present and valid for use during translation.
        pub const UNMAPPED: u32 = 0x01;

        /// If set, the page mapping is marked as read-only. If cleared, it's marked as read-write.
        pub const READ_ONLY: u32 = 0x02;

        /// If set, the page mapping has supervisor-level access permission
        pub const KERNEL: u32 = 0x04;

        /// If set, the page is marked as uncacheable.
        pub const UNCACHED: u32 = 0x08;

        /// If set, the page is marked for write-through caching. If cleared, the page is
        /// marked for write-back caching.
        pub const WRITE_THRU: u32 = 0x10;

        /// If set,the page is marked for write-combining caching.
        pub const WRITE_COMB: u32 = 0x20;

        /// If set, then it indicates that the page was written to.
        pub const DIRTY: u32 = 0x40;

        /// If set, then it indicates that a page mapping was accessed by the processor
        /// during virtual address translation.
        pub const ACCESSED: u32 = 0x80;

        /// If set, then it indicates that a non-zero-level mapping maps to a page
        /// instead of a page table (i.e. level 1 mapping maps to a large page).

        pub const PAGE_SIZED: u32 = 0x100;

        /// If set, then the page mapping will not be automatically invalidated
        /// upon a context switch.
        pub const STICKY: u32 = 0x200;

        /// If set, then the frame used for the mapping will be cleared prior to mapping.
        pub const CLEAR: u32 = 0x400;

        /// If set, then `set_page_mappings()` will not fail
        /// if an address maps to a previous entry that's already
        /// marked as present.
        pub const OVERWRITE: u32 = 0x8000;

        /// If set, then an array of physical frames
        /// is passed as input. Otherwise a pointer to a
        /// starting physical frame will be used.
        pub const ARRAY: u32 = 0x4000;
    }

    pub mod thread {
        pub const STATUS: u32 = 1;
        pub const PRIORITY: u32 = 2;
        pub const REG_STATE: u32 = 4;
        pub const PMAP: u32 = 8;
        pub const EXT_REG_STATE: u32 = 16;
    }
}

/*
pub fn sleep(timeout: u32) -> Result<()> {
    let result = unsafe { sleep(timeout) };

    match result {
        status::OK => Ok(()),
        err => Err(err)
    }
}
*/

#[derive(Copy, Clone, PartialEq, Eq, Hash)]
pub enum Resource {
    PageMapping,
    Tcb,
    Interrupt,
    Capability
}

#[derive(Copy, Clone, PartialEq, Eq, Hash)]
pub enum SleepGranularity {
    Seconds,
    Milliseconds,
    Microseconds
}

impl Default for SleepGranularity {
    fn default() -> Self {
        Self::Milliseconds
    }
}

pub const INFINITE_DURATION: u32 = 0xFFFFFFFF;
use c_types::{
    ThreadState, CreateIntArgs, CreateTcbArgs, UpdateIntArgs, UpdatePageMappingArgs, UpdateTcbArgs,
    ReadIntArgs, ReadPageMappingArgs, ReadTcbArgs, DestroyIntArgs, DestroyTcbArgs
};

pub use c_types::PageMapping;

pub enum CreateArgs {
    Tcb {
        entry: *const (),
        addr_space: Option<PAddr>,
        stack_top: *const (),
    },
    IntHandler {
        irq: u8
    },
    Capability
}

pub enum ReadArgs<'a> {
    PageMapping {
        virt: *const (),
        addr_space: Option<PAddr>,
        mappings: &'a mut [PageMapping],
        level: u32
    },
    Tcb {
        tid: Option<Tid>,
        info: &'a mut ThreadState,
        info_length: usize
    },
    IntHandler {
        mask: u32,
        blocking: bool
    },
    Capability
}

pub enum UpdateArgs<'a> {
    PageMapping {
        virt: *mut (),
        addr_space: Option<CPageMap>,
        mappings: &'a [PageMapping],
        level: u32
    },
    Tcb {
        tid: Option<Tid>,
        flags: u32,
        info: ThreadState,
    },
    IntHandler {
        mask: u32
    },
    Capability
}

pub enum DestroyArgs {
    Tcb {
        tid: Tid
    },
    IntHandler {
        irq: u8
    },
    Capability
}

pub fn create(args: &CreateArgs) -> Result<i32> {
    use CreateArgs::*;

    match args {
        Tcb { entry, addr_space, stack_top } => {
            let tcb_args = CreateTcbArgs {
                entry: *entry as *const c_void,
                addr_space: addr_space.unwrap_or(CURRENT_ROOT_PMAP),
                stack_top: *stack_top as *const c_void,
            };

            syscall_result(syscall!(
                SyscallFunction::Create, Resource::Tcb, &tcb_args as *const CreateTcbArgs as *const c_void
            ))
        },
        IntHandler { irq } => {
            let int_args = CreateIntArgs {
                irq: *irq as u32
            };

            syscall_result(syscall!(
                SyscallFunction::Create, Resource::Interrupt, &int_args as *const CreateIntArgs as *const c_void
            ))
        }
        Capability => Err(SyscallError::NotImplemented)
    }
}

pub fn read(args: &mut ReadArgs) -> Result<i32> {
    use ReadArgs::*;

    match args {
        PageMapping { virt, addr_space, mappings, level } => {
            let page_mapping_args = ReadPageMappingArgs {
                virt: *virt as *const c_void,
                count: mappings.len(),
                addr_space: addr_space.unwrap_or(CURRENT_ROOT_PMAP),
                mappings: mappings.as_mut_ptr(),
                level: *level as c_int
            };

            syscall_result(syscall!(
                SyscallFunction::Read, Resource::PageMapping, &page_mapping_args as *const ReadPageMappingArgs as *const c_void
            ))
        },
        Tcb { tid, info, info_length } => {
            let tcb_args = ReadTcbArgs {
                tid: tid.map(|t| CTid::from(t)).unwrap_or(NULL_TID),
                info: *info as *mut ThreadState,
                info_length: *info_length,
            };

            syscall_result(syscall!(
                SyscallFunction::Read, Resource::Tcb, &tcb_args as *const ReadTcbArgs as *const c_void
            ))
        },
        IntHandler { mask, blocking } => {
            let int_args = ReadIntArgs {
                int_mask: *mask,
                blocking: if *blocking { 1 } else { 0 }
            };

            syscall_result(syscall!(
                SyscallFunction::Read, Resource::Interrupt, &int_args as *const ReadIntArgs as *const c_void
            ))
        }
        Capability => Err(SyscallError::NotImplemented)
    }
}

pub fn update(args: &UpdateArgs) -> Result<i32> {
    use UpdateArgs::*;

    match args {
        PageMapping { virt, addr_space, mappings, level } => {
            let page_mapping_args = UpdatePageMappingArgs {
                virt: *virt as *mut c_void,
                count: mappings.len(),
                addr_space: addr_space.unwrap_or(CURRENT_ROOT_PMAP),
                mappings: mappings.as_ptr(),
                level: *level as c_int
            };

            syscall_result(syscall!(
                SyscallFunction::Update, Resource::PageMapping, &page_mapping_args as *const UpdatePageMappingArgs as *const c_void
            ))
        },
        Tcb { tid, flags, info } => {
            let tcb_args = UpdateTcbArgs {
                tid: tid.map(|t| CTid::from(t)).unwrap_or(NULL_TID),
                info: info as *const ThreadState,
                flags: *flags as c_uint,
            };

            syscall_result(syscall!(
                SyscallFunction::Update, Resource::Tcb, &tcb_args as *const UpdateTcbArgs as *const c_void
            ))
        },
        IntHandler { mask } => {
            let int_args = UpdateIntArgs {
                int_mask: *mask,
            };

            syscall_result(syscall!(
                SyscallFunction::Update, Resource::Interrupt, &int_args as *const UpdateIntArgs as *const c_void
            ))
        }
        Capability => Err(SyscallError::NotImplemented)
    }
}

pub fn destroy(args: &DestroyArgs) -> Result<i32> {
    use DestroyArgs::*;

    match args {
        Tcb { tid } => {
            let tcb_args = DestroyTcbArgs {
                tid: CTid::from(*tid)
            };

            syscall_result(syscall!(
                SyscallFunction::Destroy, Resource::Tcb, &tcb_args as *const DestroyTcbArgs as *const c_void
            ))
        },
        IntHandler { irq } => {
            let int_args = DestroyIntArgs {
                irq: *irq as u32
            };

            syscall_result(syscall!(
                SyscallFunction::Destroy, Resource::Interrupt, &int_args as *const DestroyIntArgs as *const c_void
            ))
        }
        Capability => Err(SyscallError::NotImplemented)
    }
}

pub fn sleep(duration: u32, granularity: SleepGranularity) -> Result<()> {
    syscall_result(syscall!(
        SyscallFunction::Sleep, duration as c_uint, granularity as c_int
    )).map(|_| ())
}

pub fn r#yield() -> Result<()> {
    sleep(0, SleepGranularity::default())
}

pub fn wait() -> Result<()> {
    sleep(INFINITE_DURATION, SleepGranularity::default())
}

pub fn send(header: &mut MessageHeader, send_buffer: &[u8]) -> Result<usize> {
    let (retval, actual_recipient, bytes_sent, _) = syscall_with_output!(
        SyscallFunction::Send,
        (header.target.map(|t| u16::from(t)).unwrap_or(NULL_TID) as u32) |  ((header.flags as u32) << 16),
        header.subject as u32,
        send_buffer.as_ptr() as *const c_void as u32,
        send_buffer.len() as u32
    );

    header.target = Tid::try_from(actual_recipient as u16).ok();

    syscall_result(retval as i32).map(|_|  bytes_sent as usize)
}

pub fn receive(header: &mut MessageHeader, recv_buffer: &mut [u8]) -> Result<usize> {
    let (retval, actual_sender_and_flags, subject, bytes_rcvd) = syscall_with_output!(
        SyscallFunction::Receive,
        (header.target.map(|t| u16::from(t)).unwrap_or(NULL_TID) as u32) |  ((header.flags as u32) << 16),
        header.subject as u32,
        recv_buffer.as_mut_ptr() as *mut c_void as u32,
        recv_buffer.len() as u32
    );

    header.target = Tid::try_from(actual_sender_and_flags as u16).ok();
    header.flags = (actual_sender_and_flags >> 16) as u16;
    header.subject = subject;

    syscall_result(retval as i32).map(|_|  bytes_rcvd as usize)
}

pub fn get_page_mappings(level: u32, virt: *const (), addr_space: Option<CPageMap>,
                             mappings: &mut [PageMapping]) -> Result<usize> {
    let mut args = ReadArgs::PageMapping {
        virt,
        addr_space,
        mappings,
        level,
    };

    read(&mut args).and_then(|count| if count as usize == mappings.len() {
        Ok(count as usize)
    } else if count == 0 {
        Err(SyscallError::Failed)
    } else {
        Err(SyscallError::PartiallyMapped(count as usize))
    })
}

pub fn set_page_mappings(level: u32, virt: *mut (), addr_space: Option<CPageMap>,
                             mappings: &[PageMapping]) -> Result<usize> {
    let args = UpdateArgs::PageMapping {
        virt,
        addr_space,
        mappings,
        level: level 
    };
    
    update(&args).and_then(|count| if count as usize == mappings.len() {
        Ok(count as usize)
    } else if count == 0 {
        Err(SyscallError::Failed)
    } else  {
        Err(SyscallError::PartiallyMapped(count as usize))
    })
}