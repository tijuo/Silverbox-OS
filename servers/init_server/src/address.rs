#![allow(dead_code)]

use core::cmp::Ordering;
use core::ops::{Sub};
use core::ptr;
use num_traits::PrimInt;

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
pub trait Align {
    type Boundary;
    type Output;

    fn is_aligned(&self, boundary: Self::Boundary) -> bool;
    fn align(&self, boundary: Self::Boundary) -> Self::Output;
    fn align_next(&self, boundary: Self::Boundary) -> Self::Output;
    fn align_trunc(&self, boundary: Self::Boundary) -> Self::Output;
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

impl<T> Align for T
where T: PrimInt {
    type Boundary = T;
    type Output = T;

    fn is_aligned(&self, boundary: Self::Boundary) -> bool {
        (*self % boundary) == T::zero()
    }

    fn align(&self, boundary: Self::Boundary) -> Self::Output {
        if !self.is_aligned(boundary) {
            self.align_next(boundary)
        } else {
            *self
        }
    }

    fn align_next(&self, boundary: Self::Boundary) -> Self::Output {
        *self + (boundary - (*self % boundary))
    }

    fn align_trunc(&self, boundary: Self::Boundary) -> Self::Output {
        if !self.is_aligned(boundary) {
            *self - (*self % boundary)
        } else {
            *self
        }
    }
}

#[cfg(test)]
mod test {
    use super::{PAddr, VAddr};
    use super::Align;
    use crate::address::AlignPtr;

    #[test]
    fn test_align() {
        let address: PAddr = 0x1000;
        let address2: PAddr = 1234;
        let address3: VAddr = 0x0000 as VAddr;
        let address4: VAddr = 1235532 as VAddr;
        let address5: VAddr = 0xFFF as VAddr;
        let address6: PAddr = 0xFFFFFF;
        let address7: PAddr = 0x200000;
        let address8: VAddr = 0x10000 as VAddr;
        let address9: VAddr = 0x3039 as VAddr;

        assert!(address.is_aligned(0x1000));
        assert!(!address2.is_aligned(0x1F));
        assert!(address3.is_aligned(0x1000));
        assert!(address4.is_aligned(1));
        assert_eq!(address5.align(4096), 0x1000usize as VAddr);
        assert_eq!(address5.align_trunc(0x1000), 0usize as VAddr);
        assert_eq!(address6.align_trunc(0x1000), 0xFFF000);
        assert_eq!(address7.align_trunc(0x1000), 0x200000);
        assert_eq!(address8.align(0x10), 0x10000usize as VAddr);
        assert_eq!(address8.align_next(0x10), 0x10010usize as VAddr);
        assert_eq!(address9.align_next(0x10), 0x3040usize as VAddr);
    }
}