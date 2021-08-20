use crate::syscall::status;
use core::prelude::v1::*;
use alloc::string::String;
use crate::lowlevel::{print_debug, print_debugln, print_debug_i32};
use crate::Tid;
use crate::syscall::c_types::{ThreadInfo};
use crate::syscall;
use crate::eprintln;

pub type Error = i32;

pub fn is_kernel_error(error: &Error) -> bool {
    *error <= 0
}

pub const BAD_REQUEST: Error = 256;
pub const KERNEL_ACCESS: Error = 257;
pub const ILLEGAL_MEM_ACCESS: Error = 258;
pub const OUT_OF_MEMORY: Error = 259;
pub const LONG_NAME: Error = 260;
pub const ALREADY_REGISTERED: Error = 261;
pub const NOT_REGISTERED: Error = 262;
pub const INVALID_NAME: Error = 263;
pub const INVALID_ADDRESS: Error = 264;
pub const ZERO_LENGTH: Error = 265;
pub const INVALID_PORT: Error = 266;
pub const NOT_IMPLEMENTED: Error = 267;
pub const INVALID_ID: Error = 268;
pub const LENGTH_OVERFLOW: Error = 269;
pub const OPERATION_FAILED: Error = 270;
pub const DEVICE_NOT_EXIST: Error = 271;
pub const END_OF_FILE: Error = 272;
pub const PARSE_ERROR: Error = 273;

pub fn log_error(code: Error, arg: Option<String>) {
    let explanation = match code {
        status::OK => "Operation completed successfully",
        status::ARG => "Bad argument to system call",
        status::FAIL => "System call failed",
        status::NOTIMPL => "System call has not been implemented yet",
        status::NOTREADY => "System call is not ready",
        status::PERM => "Caller does not have the necessary permission for the system call",
        status::BADCALL => "Invalid system call",
        BAD_REQUEST => "Invalid request",
        KERNEL_ACCESS => "Attempt to access privileged memory",
        ILLEGAL_MEM_ACCESS => "Illegal memory access",
        OUT_OF_MEMORY => "Out of memory",
        LONG_NAME => "Name is too long",
        ALREADY_REGISTERED => "Target has already been registered",
        NOT_REGISTERED => "Target has not been registered yet",
        INVALID_NAME => "Malformed name",
        INVALID_ADDRESS => "Invalid address",
        ZERO_LENGTH => "Empty buffer",
        INVALID_PORT => "Invalid port",
        NOT_IMPLEMENTED => "Not implemented",
        INVALID_ID => "Invalid id",
        LENGTH_OVERFLOW => "Length would result in overflow",
        OPERATION_FAILED => "Operation failed",
        DEVICE_NOT_EXIST => "Device does not exist",
        END_OF_FILE => "End of file",
        PARSE_ERROR => "Parse error",
        _ => "Something went wrong"
    };

    if let Some(reason) = arg {
        print_debug("Error ");
        print_debug_i32(code, 10);
        print_debug(": ");
        print_debug(explanation);
        print_debug(" - ");
        print_debug(reason.as_str());
        print_debugln(".");
        //eprintln!("Error {}: {} - {}.", code, explanation, reason);
    } else {
        print_debug("Error ");
        print_debug_i32(code, 10);
        print_debug(": ");
        print_debug(explanation);
        print_debugln(".");

        //eprintln!("Error {}. {}.", code, explanation);
    }
}

pub fn dump_state(tid: &Tid) {
    let mut thread_info = ThreadInfo::default();
    thread_info.status = ThreadInfo::READY;

    match syscall::read_thread(tid, ThreadInfo::REG_STATE) {
        Ok(thread_struct) => {

            let reg_state = thread_struct.state().unwrap();
            eprintln!("eax: {:#8x} ebx: {:#8x} ecx: {:#8x} edx: {:#8x}", reg_state.eax, reg_state.ebx, reg_state.ecx, reg_state.edx);
            eprintln!("esi: {:#8x} edi: {:#8x} ebp: {:#8x} esp: {:#8x}", reg_state.esi, reg_state.edi, reg_state.ebp, reg_state.esp);
            eprintln!("eip: {:#8x} cs:  {:#4x}     ss:  {:#4x}     eflags: {:#8x}", reg_state.eip, reg_state.cs, reg_state.ss, reg_state.eflags);
        },
        _ => {
            eprintln!("Unable to dump register state");
        }
    }
}

#[cfg(test)]
mod test {
    use super::{LONG_NAME, OUT_OF_MEMORY, INVALID_ID};
    use crate::syscall::status::{BADCALL, PERM, OK};

    #[test]
    fn test_is_kernel_error() {
        assert!(!super::is_kernel_error(&INVALID_ID));
        assert!(super::is_kernel_error(&OK));
        assert!(super::is_kernel_error(&PERM));
        assert!(super::is_kernel_error(&BADCALL));
        assert!(!super::is_kernel_error(&LONG_NAME));
        assert!(!super::is_kernel_error(&OUT_OF_MEMORY));
    }
}