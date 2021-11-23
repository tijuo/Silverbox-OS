use crate::syscalls::{self, SyscallError, PageMapping};
use crate::syscalls::c_types::CPageMap;
use crate::types::Tid;
use core::ffi::c_void;

pub fn get_root_pmap() -> syscalls::Result<CPageMap> {
    let mut mapping = [PageMapping::default()];

    syscalls::sys_get_page_mappings(2, core::ptr::null() as *const c_void,
                          None, &mut mapping)
        .and_then(|result|
            match result {
                1 => Ok(mapping[0].number << 12),
                _ => Err(SyscallError::Failed)
            })
}

pub fn get_tid() -> syscalls::Result<Tid> {
    Tid::try_from(syscalls::sys_read_thread(None)?.tid)
        .map_err(|_|SyscallError::UnspecifiedError)
}