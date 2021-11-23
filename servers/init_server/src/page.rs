use rust::align::Align;
use crate::address::{PAddr, PSize};
use rust::device::DeviceId;

pub type PageMapBase = u32;

#[derive(Debug, PartialEq, Eq, Hash, Clone, Copy)]
pub enum FrameSize {
    Small,
    PaeLarge,
    PseLarge,
    Huge,
}

impl FrameSize {
    pub const SMALL_PAGE_SIZE: usize = 0x1000;
    pub const PAE_LARGE_PAGE_SIZE: usize = 0x200000;
    pub const PSE_LARGE_PAGE_SIZE: usize = 0x400000;
    pub const HUGE_PAGE_SIZE: usize = 0x40000000;

    pub const fn bytes(&self) -> usize {
        match self {
            Self::Small => Self::SMALL_PAGE_SIZE,
            Self::PseLarge => Self::PSE_LARGE_PAGE_SIZE,
            Self::PaeLarge => Self::PAE_LARGE_PAGE_SIZE,
            Self::Huge => Self::HUGE_PAGE_SIZE,
        }
    }
}

#[derive(Debug, PartialEq, Eq, Hash, Clone, Copy)]
pub struct PhysicalFrame(PAddr, FrameSize);

impl PhysicalFrame {
    pub const SMALL_PAGE_SIZE: usize = FrameSize::SMALL_PAGE_SIZE;
    pub const PAE_LARGE_PAGE_SIZE: usize = FrameSize::PAE_LARGE_PAGE_SIZE;
    pub const PSE_LARGE_PAGE_SIZE: usize = FrameSize::PSE_LARGE_PAGE_SIZE;
    pub const HUGE_PAGE_SIZE: usize = FrameSize::HUGE_PAGE_SIZE;

    pub fn new(address: PAddr, size: FrameSize) -> Self {
        Self(address.align_trunc(size.bytes() as PSize), size)
    }

    pub fn new_from_frame(frame_number: PSize, size: FrameSize) -> Self {
        Self((frame_number * size.bytes() as PSize) as PAddr, size)
    }

    pub fn to_new_frame_size(&self, size: FrameSize) -> Self {
        Self::new(self.0, size)
    }

    pub fn components(&self) -> (PSize, usize) {
        ((self.0 / self.1.bytes() as PSize), (self.0 % self.1.bytes() as PSize) as usize)
    }

    pub fn frame(&self) -> PSize {
        self.components().0
    }

    pub fn offset(&self) -> usize {
        self.components().1
    }

    pub fn address(&self) -> PAddr { self.0 }

    pub fn frame_size(&self) -> FrameSize {
        self.1
    }

    pub fn is_valid_pmap_base(&self) -> bool {
        self.0 < PageMapBase::MAX as PAddr && self.1 == FrameSize::Small
    }
}

pub enum ConversionError {
    OutOfRange,
    WrongFrameSize
}

impl TryFrom<PhysicalFrame> for PageMapBase {
    type Error = ConversionError;

    fn try_from(value: PhysicalFrame) -> Result<Self, Self::Error> {
        if value.is_valid_pmap_base() {
            Ok(value.address() as PageMapBase)
        } else if value.frame_size() != FrameSize::Small {
            Err(ConversionError::WrongFrameSize)
        } else {
            Err(ConversionError::OutOfRange)
        }
    }
}

impl From<PhysicalFrame> for PAddr {
    fn from(page: PhysicalFrame) -> Self {
        page.address()
    }
}

#[derive(Clone, PartialEq, Eq)]
pub struct VirtualPage {
    pub device: DeviceId,
    pub offset: u64,
    pub flags: u32,
}

#[allow(dead_code)]
impl VirtualPage {
    const SWAPPED_OUT: u32 = 0x00000001;
    const SMALL_PAGE: u32 = 0x00000000;
    const LARGE_PAGE: u32 = 0x00000002;
    // 2/4 MB page
    const HUGE_PAGE: u32 = 0x00000004;
    // 1 GB page
    const UNSWAPPABLE: u32 = 0x00000008;

    pub const SMALL_PAGE_SIZE: usize = PhysicalFrame::SMALL_PAGE_SIZE as usize;
    pub const LARGE_PAGE_SIZE: usize = PhysicalFrame::PAE_LARGE_PAGE_SIZE as usize;
    pub const HUGE_PAGE_SIZE: usize = PhysicalFrame::HUGE_PAGE_SIZE as usize;

    pub(crate) fn new(device: DeviceId, offset: u64, flags: u32) -> VirtualPage {
        VirtualPage {
            device,
            offset,
            flags,
        }
    }

    pub fn size(&self) -> usize {
        match self.flags & 0x06 {
            Self::SMALL_PAGE => Self::SMALL_PAGE_SIZE,
            Self::LARGE_PAGE => Self::LARGE_PAGE_SIZE,
            Self::HUGE_PAGE => Self::HUGE_PAGE_SIZE,
            _ => Self::SMALL_PAGE_SIZE,
        }
    }

    pub fn add_offset(&self, offset2: u64) -> VirtualPage {
        Self {
            device: self.device.clone(),
            offset: self.offset + offset2,
            flags: self.flags,
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_physical_page() {
        let p1 = PhysicalFrame::new(0x1234 as PAddr, FrameSize::Small);

        assert_eq!(p1.address(), 0x1234 as PAddr);
        assert_eq!(p1.frame(), 1 as PSize);
        assert_eq!(p1.offset(), 0x234 as PSize);
        assert_eq!(p1.components(), (1, 0x234));
    }

    #[test]
    fn test_new_from_frame() {
        let frame = 55132;
        let p = PhysicalFrame::new_from_frame(frame, FrameSize::Small);

        assert_eq!(p.frame(), frame);
        assert_eq!(p.address(), 0xD75C000);
        assert_eq!(p.offset(), 0);
        assert_eq!(p.components(), (frame, 0));
    }
}
