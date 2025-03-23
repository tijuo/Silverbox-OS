#![allow(dead_code)]
use core::arch::asm;

pub unsafe fn write_u8(port: u16, data: u8) {
    unsafe {
        asm!("out dx, al", in("al") data, in("dx") port);
    }
}
pub unsafe fn write_i8(port: u16, data: i8) {
    unsafe {
        asm!("out dx, al", in("al") data, in("dx") port);
    }
}
pub unsafe fn write_u16(port: u16, data: u16) {
    unsafe {
        asm!("out dx, ax", in("ax") data, in("dx") port);
    }
}
pub unsafe fn write_i16(port: u16, data: i16) {
    unsafe {
        asm!("out dx, al", in("ax") data, in("dx") port);
    }
}
pub unsafe fn write_u32(port: u16, data: u32) {
    unsafe {
        asm!("out dx, al", in("eax") data, in("dx") port);
    }
}
pub unsafe fn write_i32(port: u16, data: i32) {
    unsafe {
        asm!("out dx, al", in("eax") data, in("dx") port);
    }
}

pub unsafe fn read_u8(port: u16, data: u8) {
    unsafe {
        asm!("in al, dx", in("al") data, in("dx") port);
    }
}
pub unsafe fn read_i8(port: u16, data: i8) {
    unsafe {
        asm!("in al, dx", in("al") data, in("dx") port);
    }
}
pub unsafe fn read_u16(port: u16, data: u16) {
    unsafe {
        asm!("in ax, dx", in("ax") data, in("dx") port);
    }
}
pub unsafe fn read_i16(port: u16, data: i16) {
    unsafe {
        asm!("in ax, dx", in("ax") data, in("dx") port);
    }
}
pub unsafe fn read_u32(port: u16, data: u32) {
    unsafe {
        asm!("in eax, dx", in("eax") data, in("dx") port);
    }
}
pub unsafe fn read_i32(port: u16, data: i32) {
    unsafe {
        asm!("in eax, dx", in("eax") data, in("dx") port);
    }
}
