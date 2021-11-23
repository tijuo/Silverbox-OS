pub use crate::types::Tid;
pub use self::c_types::{CTid, CPageMap};
use core::result;
pub use core::ffi::c_void;
use crate::syscalls::c_types::{CURRENT_ROOT_PMAP, MmxFpuRegister, MmxRegister, NULL_TID, XmmRegister};
use crate::message::MessageHeader;

macro_rules! syscall {
    (number $sc_num:expr, $($tail:tt)*) => {
        {
           syscall_with_number! { $sc_num, $($tail)* }.0
        }
    };
    ($tail:tt) => {
        compile_err("Missing system call number. Must use syscall!(number x ...)")
    }
}

macro_rules! syscall_with_output {
    (number $sc_num:expr, $($tail:tt)*) => {
        {
           syscall_with_number! { $sc_num, $($tail)* }
        }
    };
    ($tail:tt) => {
        compile_err("Missing system call number. Must use syscall!(number x ...)")
    }
}

macro_rules! syscall_with_number {
    ($sc_num:expr, word $w_arg:expr, $($tail:tt)+) => {
        {
            syscall_with_number! { ((((($w_arg) as u32) & 0xFFFF) << 16) | ((($sc_num) as u32) & 0xFF)), $($tail)* }
        }
    };
    ($sc_num:expr, byte1 $b_arg:expr, $($tail:tt)+) => {
        {
            syscall_with_number! { ((((($b_arg) as u32) & 0xFF) << 8) | ((($sc_num) as u32) & 0xFF)), $($tail)* }
        }
    };
    ($sc_num:expr, byte2 $b_arg:expr, $($tail:tt)+) => {
        {
            syscall_with_number! { ((((($b_arg) as u32) & 0xFF) << 16) | ((($sc_num) as u32) & 0xFF)), $($tail)* }
        }
    };
    ($sc_num:expr, byte3 $b_arg:expr, $($tail:tt)+) => {
        {
            syscall_with_number! { ((((($b_arg) as u32) & 0xFF) << 24) | ((($sc_num) as u32) & 0xFF)), $($tail)* }
        }
    };
    ($sc_num:expr, args [ $arg1:expr ]) => {
        {
            let mut return_value: i32;
            let mut return_args: [u32; 3] = [0; 3];
            unsafe {
                asm!(
                    "xchg ecx, esi",
                    "push ebp",
                    "push edx",
                    "push ecx",
                    "mov ecx, 2f",
                    "sysenter",
                    "2:",
                    "pop ecx",
                    "pop edx",
                    "pop ebp",
                    "xchg ecx, esi",
                    inlateout("eax") ($sc_num) as u32 => return_value,
                    inlateout("ebx") ($arg1) as u32 => return_args[0],
                    lateout("ecx") return_args[1],
                    lateout("edi") return_args[2],
                );
            }
            (return_value, return_args[0], return_args[1], return_args[2])
        }
    };
    ($sc_num:expr, args [ $arg1:expr, $arg2:expr ]) => {
        {
            let mut return_value: i32;
            let mut return_args: [u32; 3] = [0; 3];

            unsafe {
                asm!(
                    "xchg ecx, esi",
                    "push ebp",
                    "push edx",
                    "push ecx",
                    "mov ecx, 2f",
                    "sysenter",
                    "2:",
                    "pop ecx",
                    "pop edx",
                    "pop ebp",
                    "xchg ecx, esi",
                    inlateout("eax") ($sc_num) as u32 => return_value,
                    inlateout("ebx") ($arg1) as u32=> return_args[0],
                    in("edx") ($arg2) as u32,
                    lateout("ecx") return_args[1],
                    lateout("edi") return_args[2],
                );
            }
            (return_value, return_args[0], return_args[1], return_args[2])
        }
    };
    ($sc_num:expr, args [ $arg1:expr, $arg2:expr, $arg3:expr ]) => {
        {
            let mut return_value: i32;
            let mut return_args: [u32; 3] = [0; 3];

            unsafe {
                asm!(
		    "xchg ecx, esi",
                    "push ebp",
                    "push edx",
                    "push ecx",
                    "mov ecx, 2f",
                    "sysenter",
                    "2:",
                    "pop ecx",
                    "pop edx",
                    "pop ebp",
		    "xchg ecx, esi",
                    inlateout("eax") ($sc_num) as u32 => return_value,
                    inlateout("ebx") ($arg1) as u32 => return_args[0],
                    in("edx") ($arg2) as u32,
                    inlateout("ecx") ($arg3) as u32 => return_args[1],
                    lateout("edi") return_args[2],
                );
            }
            (return_value, return_args[0], return_args[1], return_args[2])
        }
    };
    ($sc_num:expr, args [ $arg1:expr, $arg2:expr, $arg3:expr, $arg4:expr ]) => {
        {
            let mut return_value: i32;
            let mut return_args: [u32; 3] = [0; 3];

            unsafe {
                asm!(
                    "xchg ecx, esi",
                    "push ebp",
                    "push edx",
                    "push ecx",
                    "mov ecx, 2f",
                    "sysenter",
                    "2:",
                    "pop ecx",
                    "pop edx",
                    "pop ebp",
                    "xchg ecx, esi",
                    inlateout("eax") ($sc_num) as u32 => return_value,
                    inlateout("ebx") ($arg1) as u32 => return_args[0],
                    in("edx") ($arg2) as u32,
                    inlateout("ecx") ($arg3) as u32 => return_args[1],
                    inlateout("edi") ($arg4) as u32 => return_args[2]
                );
            }
            (return_value, return_args[0], return_args[1], return_args[2])
        }
    };
    ($sc_num:expr,?) => {
        {
            let mut return_value: i32;
            let mut return_arg: u32;

            unsafe {
                asm!(
                    "push ebp",
                    "push edx",
                    "push ecx",
                    "mov ecx, 2f",
                    "sysenter",
                    "2:",
                    "pop ecx",
                    "pop edx",
                    "pop ebp",
                    inlateout("eax") ($sc_num) as u32 => return_value,
                    lateout("ebx") return_arg
                );
            }
            (return_value, return_args[0], return_args[1], return_args[2])
        }
    };
}

pub const SEND: u32 = 2;
pub const RECV: u32 = 3;
pub const SEND_AND_RECV: u32 = 4;
pub const GET_PAGE_MAPPINGS: u32 = 5;
pub const SET_PAGE_MAPPINGS: u32 = 6;
pub const CREATE_THREAD: u32 = 7;
pub const DESTROY_THREAD: u32 = 8;
pub const READ_THREAD: u32 = 9;
pub const UPDATE_THREAD: u32 = 10;
pub const POLL: u32 = 11;
pub const EOI: u32 = 12;

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
    PartiallyMapped(usize),
    UnspecifiedError
}

fn syscall_result(value: i32) -> Result<i32> {
    match value {
        x if x >= 0 => Ok(x),
        status::ARG => Err(SyscallError::InvalidArgument),
        status::FAIL => Err(SyscallError::Failed),
        status::PERM => Err(SyscallError::Unauthorized),
        status::BADCALL => Err(SyscallError::InvalidSystemCall),
        status::NOTIMPL => Err(SyscallError::NotImplemented),
        status::INT => Err(SyscallError::Interrupted),
        _ => Err(SyscallError::UnspecifiedError),
    }
}

pub mod c_types {
    pub type CAddr = u32;
    pub type CTid = u16;
    pub type CPageMap = u32;

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

    pub mmx_st: [c_types::MmxFpuRegister; 8],
    pub xmm: [c_types::XmmRegister; 8],
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
    pub parent: CTid,

    pub next_sibling: CTid,
    pub prev_sibling: CTid,

    pub children_head: CTid,
    pub children_tail: CTid,

    pub receiver_wait_head: CTid,
    pub receiver_wait_tail: CTid,

    pub sender_wait_head: CTid,
    pub sender_wait_tail: CTid,

    pub ext_register_state: LegacyXSaveState,
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
            parent: 0,
            next_sibling: 0,
            prev_sibling: 0,
            children_head: 0,
            children_tail: 0,
            receiver_wait_head: 0,
            receiver_wait_tail: 0,
            sender_wait_head: 0,
            sender_wait_tail: 0,
            ext_register_state: Default::default()
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

pub mod status {
    pub const OK: i32 = 0;
    pub const ARG: i32 = -1;
    pub const FAIL: i32 = -2;
    pub const PERM: i32 = -3;
    pub const BADCALL: i32 = -4;
    pub const NOTIMPL: i32 = -5;
    pub const NOTREADY: i32 = -6;
    pub const INT: i32 = -7;
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

        /// If set, then `sys_set_page_mappings()` will not fail
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
    let result = unsafe { sys_sleep(timeout) };

    match result {
        status::OK => Ok(()),
        err => Err(err)
    }
}
*/

pub fn sys_read_thread(tid: Option<&Tid>) -> Result<ThreadState> {
    let mut state = ThreadState::default();

    syscall_result(syscall!(
        number READ_THREAD,
        args [ tid.map(|t| CTid::from(t)).unwrap_or(NULL_TID),
              &mut state as *mut ThreadState as usize as u32 ]))
        .map(|_| state)
}

pub fn sys_update_thread(tid: Option<&Tid>, flags: u32, thread_state: &ThreadState) -> Result<()> {
    syscall_result(syscall!(
        number UPDATE_THREAD,
        args [ tid.map(|t| CTid::from(t)).unwrap_or(NULL_TID),
            flags,
            thread_state as *const ThreadState as usize as u32 ]))
        .map(|_| ())
}

/// Synchronously send the contents of the XMM registers to the recipient

pub fn sys_send(recipient: &Tid, subject: u32, flags: u32) -> Result<()> {
    syscall_result(syscall!(number SEND, args [ CTid::from(recipient), subject, flags ]))
        .map(|_| ())
}


/// Synchronously receive the contents of the XMM registers from the sender

pub fn sys_recv(sender: Option<Tid>, flags: u32) -> Result<MessageHeader> {
    let (retval, actual_sender, subj, send_flags) = syscall_with_output!(
        number RECV,
        args [ sender.map_or(NULL_TID, |s| CTid::from(s)), flags ]);

    let actual_sender = Tid::try_from(actual_sender as CTid)
                            .map_err(|_| SyscallError::UnspecifiedError)?;

    syscall_result(retval)
        .map(|_| MessageHeader::new_from_sender(
                          &actual_sender,
                          subj,
                          send_flags))
}

/// Synchronously send the contents of the XMM registers to the recipient and then immediately
/// synchronously receive the contents of the XMM registers from the replier

pub fn sys_send_and_recv(recipient: &Tid, replier: Option<&Tid>, subject: u32, send_flags: u32,
                         recv_flags: u32) -> Result<MessageHeader> {
    let recipient_replier = ((replier
        .map_or(NULL_TID,|r| CTid::from(r)) as u32) << 16)
        | CTid::from(recipient) as u32;

    let (retval, actual_replier, recv_subject, recv_msg_flags) = syscall_with_output!(
        number SEND_AND_RECV,
        args [ recipient_replier, subject, send_flags, recv_flags ]
    );

    let actual_replier = Tid::try_from(actual_replier as CTid)
                            .map_err(|_| SyscallError::UnspecifiedError)?;

    syscall_result(retval)
        .map(|_| MessageHeader::new_from_sender(
                          &actual_replier,
                          recv_subject,
                          recv_msg_flags))
}

pub fn sys_reply_and_wait(recipient: &Tid, subject: u32, send_flags: u32,
                          recv_flags: u32) -> Result<MessageHeader> {
    sys_send_and_recv(recipient, None, subject, send_flags,
                      recv_flags)
}

pub fn sys_get_page_mappings(level: u32, addr: *const c_void, addr_space: Option<CPageMap>,
                             mappings: &mut [PageMapping]) -> Result<usize> {
    syscall_result(syscall!(
        number GET_PAGE_MAPPINGS,
        word level,
        args [ addr as usize, mappings.len(), addr_space.unwrap_or(CURRENT_ROOT_PMAP), mappings.as_ptr() ])
    )
        .and_then(|x| if x as usize == mappings.len() {
            Ok(x as usize)
        } else if x > 0 {
            Err(SyscallError::PartiallyMapped(x as usize))
        } else {
            Err(SyscallError::Failed)
        })
}

pub fn sys_set_page_mappings(level: u32, addr: *const c_void, addr_space: Option<CPageMap>,
                             mappings: &[PageMapping]) -> Result<usize> {
    syscall_result(syscall!(
        number SET_PAGE_MAPPINGS,
        word level,
        args [ addr as usize, mappings.len(), addr_space.unwrap_or(CURRENT_ROOT_PMAP), mappings.as_ptr() ])
    )
        .and_then(|x| if x as usize == mappings.len() {
            Ok(x as usize)
        } else if x > 0 {
            Err(SyscallError::PartiallyMapped(x as usize))
        } else {
            Err(SyscallError::Failed)
        })
}

pub fn sys_create_thread(entry: * const c_void, root_pmap: Option<CPageMap>,
                         stack_top: * const c_void) -> Result<Tid> {
    syscall_result(syscall!(
        number CREATE_THREAD,
        args [ entry, root_pmap.unwrap_or(CURRENT_ROOT_PMAP), stack_top ])
    )
        .and_then(|tid| Tid::try_from(tid as u16)
            .map_err(|_| SyscallError::Failed))
}
