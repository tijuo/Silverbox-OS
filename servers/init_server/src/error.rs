use rust::syscalls::status;
use alloc::string::String;
use crate::lowlevel::{print_debug, print_debugln, print_debug_i32};
use rust::message::kernel::ExceptionMessagePayload;
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
pub const BAD_ARGUMENT: Error = 274;
pub const WOULD_BLOCK: Error = 275;
pub const INTERRUPTED: Error = 276;

pub fn log_error(code: Error, arg: Option<String>) {
    let explanation = match code {
        status::OK => "Operation completed successfully",
        status::ARG => "Bad argument to system call",
        status::INT => "System call was interrupted",
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
        BAD_ARGUMENT => "Bad argument",
        WOULD_BLOCK => "Operation would block",
        INTERRUPTED => "Operation was interrupted",
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

fn exception_name(fault_num: u8) -> &'static str {
    match fault_num {
        0 => "Divide by Zero",
        1 => "Debug Exception",
        2 => "NMI Interrupt",
        3 => "Breakpoint",
        4 => "Overflow",
        5 => "BOUND Range Exceeded",
        6 => "Invalid Opcode",
        7 => "Device Not Available",
        8 => "Double Fault",
        9 => "Coprocessor Segment Overrun",
        10 => "Invalid TSS",
        11 => "Segment Not Present",
        12 => "Stack-Segment Fault",
        13 => "General Protection Fault",
        14 => "Page Fault",
        16 => "Math Fault",
        17 => "Alignment Check",
        18 => "Machine Check",
        19 => "SIMD Floating-Point Exception",
        20 => "Virtualization Exception",
        21 => "Control Protection Exception",
        _ => ""
    }
}

fn has_exception_error_code(fault_num: u8) -> bool {
    match fault_num {
        8 | 10 ..= 14 | 17 | 21 => true,
        _ => false
    }
}

pub fn dump_state(data: &ExceptionMessagePayload) {
    eprintln!("Exception {} {}", data.fault_num, exception_name(data.fault_num));

    if has_exception_error_code(data.fault_num) {
        eprintln!("with error code: {:#x}.", data.error_code);
    }
    eprintln!("tid: {} processor id: {:#2x}", data.who, data.processor_id);
    eprintln!();
    eprintln!("eax: {:#8x} ebx: {:#8x} ecx: {:#8x} edx: {:#8x} eip: {:#8x}",
              data.eax, data.ebx, data.ecx, data.edx, data.eip);
    eprintln!("esi: {:#8x} edi: {:#8x} ebp: {:#8x} esp: {:#8x} eflags: {:#8x}",
              data.esi, data.edi, data.ebp, data.esp, data.eflags);
    eprintln!("cs: {:#4x} ds: {:#4x} es: {:#4x} fs: {:#4x} gs: {:#4x} ss: {:#4x}",
              data.cs, data.ds, data.es, data.fs, data.gs, data.ss);
    eprintln!("cr0: {:#8x} cr2: {:#8x} cr3: {:#8x} cr4: {:#8x}",
              data.cr0, data.cr2, data.cr3, data.cr4);
}

#[cfg(test)]
mod test {
    use super::{LONG_NAME, OUT_OF_MEMORY, INVALID_ID};
    use rust::syscalls::status;

    #[test]
    fn test_is_kernel_error() {
        assert!(!super::is_kernel_error(&INVALID_ID));
        assert!(super::is_kernel_error(&status::OK));
        assert!(super::is_kernel_error(&status::PERM));
        assert!(super::is_kernel_error(&status::BADCALL));
        assert!(!super::is_kernel_error(&LONG_NAME));
        assert!(!super::is_kernel_error(&OUT_OF_MEMORY));
    }
}