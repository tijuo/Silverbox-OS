use core::fmt::{self, Formatter};

use core::fmt::Display;
use rust::message::kernel::ExceptionMessage;
use crate::eprintfln;
use alloc::borrow::Cow;

/*
pub fn is_kernel_error(error: &Error) -> bool {
    *error <= 0
}
*/

/* 
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
*/

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Error {
    Ok,
    BadArgument,
    Interrupted,
    Failed,
    NotImplemented,
    NotReady,
    NotPermitted,
    InvalidSyscall,
    BadRequest,
    KernelAccess,
    IllegalMemoryAccess,
    OutOfMemory,
    TooLong,
    AlreadyRegistered,
    NotRegistered,
    InvalidName,
    InvalidAddress,
    ZeroLength,
    InvalidPort,
    InvalidId,
    Overflow,
    DoesntExist,
    EndOfFile,
    ParseError,
    WouldBlock,
    Incomplete,
}

impl Display for Error {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        use Error::*;

        let s = match self {
            Ok => "The operation completed successfully",
            BadArgument => "Bad or invalid argument",
            Interrupted => "The operation was interrupted",
            Failed => "The operation failed",
            NotImplemented => "Functionality has not been implemented yet",
            NotReady => "The operation is not ready",
            NotPermitted => "The caller does not have the necessary permissions",
            InvalidSyscall => "Invalid system call",
            BadRequest => "Invalid request",
            KernelAccess => "Attempt to access privileged memory",
            IllegalMemoryAccess => "Illegal memory access",
            OutOfMemory => "Out of memory",
            TooLong => "The argument or parameter is too long",
            AlreadyRegistered => "The target has already been registered",
            NotRegistered => "The target has not been registered yet",
            InvalidName => "Malformed name",
            InvalidAddress => "Invalid address",
            ZeroLength => "Empty buffer",
            InvalidPort => "Invalid port",
            InvalidId => "Invalid id",
            Overflow => "The operation would result in an overflow",
            DoesntExist => "The target does not exist",
            EndOfFile => "End of file",
            ParseError => "Parse error",
            WouldBlock => "The operation would block",
            Incomplete => "The operation did not complete fully"
        };

        f.write_str(s)
    }
}

impl core::error::Error for Error {}

pub fn log_error<'a>(code: Error, arg: Cow<'a, str>) {
    eprintfln!("Error {}: {} - {}.", code as i32, code, arg);
}

const fn exception_name(fault_num: u8) -> &'static str {
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

const fn has_exception_error_code(fault_num: u8) -> bool {
    match fault_num {
        8 | 10 ..= 14 | 17 | 21 => true,
        _ => false
    }
}

pub fn dump_state(data: &ExceptionMessage) {
    eprintfln!("Exception {} {}", data.fault_num, exception_name(data.fault_num));

    if has_exception_error_code(data.fault_num) {
        eprintfln!("with error code: {:#x}.", data.error_code);
    }
    eprintfln!("tid: {} processor id: {:#2x}", data.who, data.processor_id);
    eprintfln!();
    eprintfln!("eax: {:#8x} ebx: {:#8x} ecx: {:#8x} edx: {:#8x} eip: {:#8x}",
              data.eax, data.ebx, data.ecx, data.edx, data.eip);
    eprintfln!("esi: {:#8x} edi: {:#8x} ebp: {:#8x} esp: {:#8x} eflags: {:#8x}",
              data.esi, data.edi, data.ebp, data.esp, data.eflags);
    eprintfln!("cs: {:#4x} ds: {:#4x} es: {:#4x} fs: {:#4x} gs: {:#4x} ss: {:#4x}",
              data.cs, data.ds, data.es, data.fs, data.gs, data.ss);
    eprintfln!("cr0: {:#8x} cr2: {:#8x} cr3: {:#8x} cr4: {:#8x}",
              data.cr0, data.cr2, data.cr3, data.cr4);
}