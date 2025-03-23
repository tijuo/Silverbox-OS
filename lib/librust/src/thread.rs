use crate::syscalls::{self, SyscallError, CreateArgs, ReadArgs};
use crate::syscalls::c_types::{CPageMap, PageMapping, ThreadState};
use crate::types::Tid;

pub fn get_root_pmap() -> syscalls::Result<CPageMap> {
    let mut mapping = [PageMapping::default()];

    syscalls::get_page_mappings(2, core::ptr::null(),
                          None, &mut mapping)
        .and_then(|result|
            match result {
                1 => Ok(mapping[0].number << 12),
                _ => Err(SyscallError::Failed)
            })
}

pub fn get_tid() -> syscalls::Result<Tid> {
    Tid::try_from(read_thread(None)?.tid)
        .map_err(|_|SyscallError::UnspecifiedError)
}

pub fn create_thread(entry: *const (), addr_space: Option<CPageMap>,
                         stack_top: *const ()) -> syscalls::Result<Tid> {
    let args = CreateArgs::Tcb {
        entry,
        addr_space,
        stack_top,
    };

    syscalls::create(&args)
        .and_then(|tid| Tid::try_from(tid as u16)
            .map_err(|_| SyscallError::Failed))
}

pub fn read_thread(tid: Option<Tid>) -> syscalls::Result<ThreadState> {
    let mut thread_state = ThreadState::default();

    let mut args = ReadArgs::Tcb {
        tid,
        info: &mut thread_state,
        info_length: core::mem::size_of::<ThreadState>(),
    };

    syscalls::read(&mut args).map_err(|_| SyscallError::Failed).map(move |_| thread_state)
}