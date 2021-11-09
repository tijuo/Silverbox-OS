use crate::address::{PAddr, PSize};
use crate::device::DeviceId;

#[derive(Debug, PartialEq, Eq, Hash, Clone, Copy)]
pub struct PhysicalPage(PAddr);

impl PhysicalPage {
    pub const SMALL_PAGE_SIZE: PSize = 0x1000;
    pub const PAE_LARGE_PAGE_SIZE: PSize = 0x200000;
    pub const PSE_LARGE_PAGE_SIZE: PSize = 0x400000;
    pub const HUGE_PAGE_SIZE: PSize = 0x40000000;

    pub fn new(address: PAddr) -> Self {
        Self(address)
    }

    pub fn components(&self) -> (u64, u64) {
        (self.0 / Self::SMALL_PAGE_SIZE, self.0 % Self::SMALL_PAGE_SIZE)
    }

    pub fn frame(&self) -> u64 {
        self.components().0
    }

    pub fn offset(&self) -> u64 {
        self.components().1
    }

    pub fn as_address(&self) -> PAddr { self.0 }
}

impl From<PAddr> for PhysicalPage {
    fn from(addr: PAddr) -> Self {
        PhysicalPage::new(addr)
    }
}

impl From<(u64, u64)> for PhysicalPage {
    fn from(components: (u64, u64)) -> Self {
        Self((components.0 as PAddr * Self::SMALL_PAGE_SIZE as PAddr
            + components.1 as PAddr) as PAddr)
    }
}

impl From<PhysicalPage> for PAddr {
    fn from(page: PhysicalPage) -> Self {
        page.as_address()
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

    pub const SMALL_PAGE_SIZE: usize = PhysicalPage::SMALL_PAGE_SIZE as usize;
    pub const LARGE_PAGE_SIZE: usize = PhysicalPage::PAE_LARGE_PAGE_SIZE as usize;
    pub const HUGE_PAGE_SIZE: usize = PhysicalPage::HUGE_PAGE_SIZE as usize;

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
        let p1 = PhysicalPage::new(0x1234 as PAddr);

        assert_eq!(p1.as_address(), 0x1234 as PAddr);
        assert_eq!(p1.frame(), 1 as PSize);
        assert_eq!(p1.offset(), 0x234 as PSize);
        assert_eq!(p1.components(), (1, 0x234));
    }

    #[test]
    fn test_new_from_components() {
        let components = (300,816);
        let p = PhysicalPage::from(components.clone());

        assert_eq!(p.frame(), 300);
        assert_eq!(p.offset(), 816);
        assert_eq!(p.as_address(), 0x12C330);
        assert_eq!(&p.components(), &components);
    }

    #[test]
    fn test_new_from_frame() {
        let frame = 55132;
        let p = PhysicalPage::from_frame(frame);

        assert_eq!(p.frame(), frame);
        assert_eq!(p.as_address(), 0xD75C000);
        assert_eq!(p.offset(), 0);
        assert_eq!(p.components(), (frame, 0));
    }
}
