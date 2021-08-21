use crate::page::VirtualPage;
use crate::address::{VAddr, PAddr, AlignPtr, Align};
use alloc::collections::{BTreeMap, BTreeSet};
use crate::Tid;
use crate::device::DeviceId;
use core::prelude::v1::*;
use crate::error;
use core::cmp;

pub mod manager {
    use alloc::collections::BTreeMap;
    use crate::address::PAddr;
    use super::AddrSpace;
    use crate::Tid;
    use crate::syscall::{self, INIT_TID};
    use crate::error;

    static mut ADDRESS_SPACES: Option<BTreeMap<PAddr, AddrSpace>> = None;

    pub fn init() {
        unsafe {
            ADDRESS_SPACES = match ADDRESS_SPACES {
                Some(_) => panic!("Address space map has already been initialized."),
                None => Some(BTreeMap::new()),
            }
        }

        match syscall::get_init_pmap() {
            Ok(page) => {
                let mut addr_space = AddrSpace::new(page.as_address());
                addr_space.attach_thread(Tid::new(INIT_TID));

                if !register(addr_space) {
                    panic!("Unable to register the initial address space.");
                }
            },
            Err(e) => {
                error::log_error(e, None);
                panic!("Unable to initialize the address mapper.");
            }
        }
    }

    fn addr_space_map<'a>() -> &'static BTreeMap<PAddr, AddrSpace> {
        unsafe {
            ADDRESS_SPACES.as_ref()
                .expect("Address space map hasn't been initialized yet.")
        }
    }

    fn addr_space_map_mut<'a>() -> &'static mut BTreeMap<PAddr, AddrSpace> {
        unsafe {
            ADDRESS_SPACES.as_mut()
                .expect("Address space map hasn't been initialized yet.")
        }
    }

    pub fn lookup_tid<'a>(tid: &Tid) -> Option<&'a AddrSpace> {
        addr_space_map()
            .values()
            .find(|addr_space| addr_space.thread_exists(tid))
    }

    pub fn lookup_tid_mut<'a>(tid: &Tid) -> Option<&'a mut AddrSpace> {
        addr_space_map_mut()
            .values_mut()
            .find(|addr_space| addr_space.thread_exists(tid))
    }

    pub fn register(addr_space: AddrSpace) -> bool {
        let aspace_map = addr_space_map_mut();

        if aspace_map.get(&addr_space.root_page_map).is_none() {
            aspace_map.insert(addr_space.root_page_map, addr_space);
            true
        } else {
            false
        }
    }

    pub fn unregister(pmap: PAddr) -> Option<AddrSpace> {
        addr_space_map_mut().remove(&pmap)
    }
}

pub struct AddressMapping {
    pub page: Option<VirtualPage>,
    pub flags: u32,
}

impl AddressMapping {
    fn new(page: Option<VirtualPage>, flags: u32) -> AddressMapping {
        AddressMapping {
            page,
            flags,
        }
    }
}

pub struct AddrSpace {
    root_page_map: PAddr,
    vaddr_map: BTreeMap<VAddr, AddressMapping>,
    attached_threads: BTreeSet<Tid>,
}

impl AddrSpace {
    pub const READ_ONLY: u32 = 0x00000001;
    pub const NO_EXECUTE: u32 = 0x00000002;
    pub const COPY_ON_WRITE: u32 = 0x00000004;
    pub const GUARD: u32 = 0x00000008;
    pub const EXTEND_DOWN: u32 = 0x00000010;

    pub fn new(root_pmap: PAddr) -> Self {
        Self {
            root_page_map: root_pmap,
            vaddr_map: BTreeMap::new(),
            attached_threads: BTreeSet::new(),
        }
    }

    pub fn root_pmap(&self) -> PAddr {
        self.root_page_map
    }

    pub fn attach_thread(&mut self, tid: Tid) -> bool {
        self.attached_threads.insert(tid)
    }

    pub fn thread_exists(&self, tid: &Tid) -> bool {
        self.attached_threads.contains(tid)
    }

    pub fn detach_thread(&mut self, tid: &Tid) -> bool {
        self.attached_threads.remove(tid)
    }

    pub fn get_mapping(&self, addr: &VAddr) -> Option<&AddressMapping> {
        let addr = (*addr as usize).align_trunc(VirtualPage::SMALL_PAGE_SIZE) as VAddr;
        self.vaddr_map.get(&addr)
    }

    /// Maps a region of memory to some block device in a particular address space.
    ///
    /// If the desired address is `None`, then pick any available address and map to it.

    pub fn map(&mut self, addr: Option<VAddr>, dev_id: &DeviceId, offset: usize, flags: u32,
               length: usize) -> Option<VAddr> {

        if let Some(start_address) = addr {
            /*eprintln!("Mapping {:p} -> device: {}:{} offset: {} flags: {} length: {} pmap: {:#x}",
                      start_address, dev_id.major, dev_id.minor, offset, flags, length, self.root_page_map);
*/
            let end_address = start_address.wrapping_add(length)
                .align(VirtualPage::SMALL_PAGE_SIZE);
            let start_address = start_address.align_trunc(VirtualPage::SMALL_PAGE_SIZE);

            if end_address < start_address {
                None
            } else {
                let page_count = (end_address as usize - start_address as usize) / VirtualPage::SMALL_PAGE_SIZE;

                for i in 0..page_count {
                    let a: VAddr = start_address.wrapping_add(i * VirtualPage::SMALL_PAGE_SIZE);

                    if self.vaddr_map.contains_key(&a) {
                        return None
                    }
                }

                for i in 0..page_count {
                    let a: VAddr = start_address.wrapping_add(i * VirtualPage::SMALL_PAGE_SIZE);
                    let vpage = VirtualPage::new(dev_id.clone(), offset + i * VirtualPage::SMALL_PAGE_SIZE, 0);
                    let new_mapping = AddressMapping::new(Some(vpage), flags);

                    self.vaddr_map.insert(a, new_mapping);
                }

                Some(start_address)
            }
        } else {
            /* TODO: Needs to be implemented. */
            None
        }
    }

    pub fn unmap(&mut self, start_address: VAddr, length: usize) -> bool {
        let end_address = (start_address.wrapping_add(length)).align(VirtualPage::SMALL_PAGE_SIZE);
        let start_address = start_address.align_trunc(VirtualPage::SMALL_PAGE_SIZE);

        if end_address > start_address {
            let page_count = (end_address as usize - start_address as usize) / VirtualPage::SMALL_PAGE_SIZE;

            for offset in (0..page_count).step_by(VirtualPage::SMALL_PAGE_SIZE) {
                let a: VAddr = start_address.wrapping_add(offset);

                if !self.vaddr_map.contains_key(&a) {
                    return false;
                }
            }

            for offset in (0..page_count).step_by(VirtualPage::SMALL_PAGE_SIZE) {
                let a: VAddr = start_address.wrapping_add(offset);

                self.vaddr_map.remove(&a);
            }

            true
        } else {
            false
        }
    }
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct MemoryRegion {
    start: VAddr,
    length: usize,
}

impl MemoryRegion {
    /// Create a new `MemoryRegion` with a particular base address and region length.
    /// Returns `error::Error` upon failure.

    pub fn new(start: VAddr, length: usize) -> Result<MemoryRegion, error::Error> {
        let end = start.wrapping_add(length);

        if end < start {
            Err(error::LENGTH_OVERFLOW)
        } else {
            Ok(Self { start, length })
        }
    }

    /// Returns `true` if the other region is either immediately before or after this one.

    pub fn is_adjacent(&self, other: &MemoryRegion) -> bool {
        self.start.wrapping_add(self.length) == other.start ||
            other.start.wrapping_add(other.length) == self.start
    }

    /// Returns `true` if this region overlaps with the other.

    pub fn overlaps(&self, other: &MemoryRegion) -> bool {
        (other.start >= self.start && other.start < self.start.wrapping_add(self.length))
            || (self.start >= other.start && self.start < other.start.wrapping_add(other.length))
    }

    /// Returns `true` if the address is contained within the memory region.

    pub fn contains(&self, addr: VAddr) -> bool {
        addr >= self.start && addr < self.start.wrapping_add(self.length)
    }

    /// Concatenates two regions into one contiguous region.
    /// Returns the joined `MemoryRegion` on success.
    /// Returns `Err` if the two regions are neither overlapping nor adjacent.

    pub fn join(&self, other: &MemoryRegion) -> Result<MemoryRegion, i32> {
        let start = cmp::min(self.start, other.start);
        let self_end = self.start.wrapping_add(self.length);
        let other_end = other.start.wrapping_add(other.length);
        let end = cmp::max(self_end, other_end);

        if (self.is_adjacent(&other) || self.overlaps(&other)) && end >= start {
            MemoryRegion::new(start, end as usize - start as usize)
        } else {
            Err(error::OPERATION_FAILED)
        }
    }
}

#[cfg(test)]
mod test {
    use super::MemoryRegion;
    use crate::address::VAddr;

    #[test]
    fn test_join_region() {
        let region1 = MemoryRegion::new(0x1000 as usize as VAddr, 0x1000);
        let region2 = MemoryRegion::new(0x2000 as usize as VAddr, 0x1000);
        let region3 = MemoryRegion::new(0x1000 as usize as VAddr, 0x2000);
        let region4 = MemoryRegion::new(0x1800 as usize as VAddr, 0x1800);
        let region5 = MemoryRegion::new(0x1000 as usize as VAddr, 0x3000);

        assert!(region1.is_ok());
        assert!(region2.is_ok());
        assert!(region3.is_ok());
        assert!(region4.is_ok());
        assert!(region5.is_ok());

        let region1 = region1.unwrap();
        let region2 = region2.unwrap();
        let region3 = region3.unwrap();
        let region4 = region4.unwrap();
        let region5 = region5.unwrap();

        assert_ne!(&region1, &region2);
        assert_ne!(&region2, &region3);
        assert_ne!(&region1, &region3);

        let joined_region1 = region1.join(&region2);
        let joined_region2 = region1.join(&region4);

        assert!(joined_region1.is_ok());
        assert!(joined_region2.is_ok());

        let joined_region1 = joined_region1.unwrap();
        let joined_region2 = joined_region2.unwrap();

        assert_eq!(&joined_region1, &region3);
        assert_eq!(&joined_region2, &region3);
        assert_ne!(&joined_region2, &region5);
    }
}
