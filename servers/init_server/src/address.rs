#![allow(dead_code)]

use core::cmp::Ordering;
use core::ops::Sub;
use core::ptr;

pub type VAddr = * const u8;
pub type PAddr = u64;
pub type VSize = usize;
pub type VsSize = isize;
pub type PSize = u64;

pub type FrameOffset = u32;

pub const NULL_VADDR: VAddr = ptr::null();

fn difference<T: Sub<Output=T> + Ord>(a: T, b: T) -> (T, Ordering) {
    match a.cmp(&b)
    {
        Ordering::Greater => (a - b, Ordering::Greater),
        Ordering::Less => (b - a, Ordering::Less),
        Ordering::Equal => (b - a, Ordering::Equal)
    }
}

pub fn u32_into_vaddr(val: u32) -> VAddr {
    val as usize as VAddr
}

pub trait AlignPtr {
    type Boundary;
    type Output;

    fn is_aligned(&self, boundary: Self::Boundary) -> bool;
    fn align(&self, boundary: Self::Boundary) -> *const Self::Output;
    fn align_next(&self, boundary: Self::Boundary) -> *const Self::Output;
    fn align_trunc(&self, boundary: Self::Boundary) -> *const Self::Output;
}

impl<T> AlignPtr for *const T {
    type Boundary = usize;
    type Output = T;

    fn is_aligned(&self, boundary: Self::Boundary) -> bool {
        self.align_offset(boundary) == 0
    }

    fn align(&self, boundary: Self::Boundary) -> *const Self::Output {
        if !self.is_aligned(boundary) {
            self.align_next(boundary)
        } else {
            self.cast()
        }
    }

    fn align_next(&self, boundary: Self::Boundary) -> *const Self::Output {
        self.wrapping_add(self.align_offset(boundary))
    }

    fn align_trunc(&self, boundary: Self::Boundary) -> *const Self::Output {
        if !self.is_aligned(boundary) {
            self.wrapping_sub(boundary - self.align_offset(boundary))
        } else {
            self.cast()
        }
    }
}

impl<T> AlignPtr for *mut T {
    type Boundary = usize;
    type Output = T;

    fn is_aligned(&self, boundary: Self::Boundary) -> bool {
        self.align_offset(boundary) == 0
    }

    fn align(&self, boundary: Self::Boundary) -> *const Self::Output {
        if !self.is_aligned(boundary) {
            self.align_next(boundary)
        } else {
            self.cast()
        }
    }

    fn align_next(&self, boundary: Self::Boundary) -> *const Self::Output {
        self.wrapping_add(self.align_offset(boundary))
    }

    fn align_trunc(&self, boundary: Self::Boundary) -> *const Self::Output {
        if !self.is_aligned(boundary) {
            self.wrapping_sub(boundary - self.align_offset(boundary))
        } else {
            self.cast()
        }
    }
}