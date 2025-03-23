#![no_std]
#![allow(dead_code)]
#![feature(c_size_t)]
#![feature(slice_as_array)]

extern crate core;
#[macro_use] extern crate alloc;
//extern crate num_traits;

pub mod message;
pub mod types;
pub mod align;
pub mod syscalls;
pub mod io;
pub mod device;
pub mod thread;
