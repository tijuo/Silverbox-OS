use crate::page::{PageMapBase, VirtualPage};
use crate::address::VAddr;
use alloc::collections::btree_set::BTreeSet;
use rust::align::Align;
use rust::device::DeviceId;
use crate::region::{MemoryRegion, RegionSet};
use core::cmp::Ordering;
pub use rust::types::Tid;

pub mod manager {
    use rust::types::Tid;
    use super::AddrSpace;
    use crate::error;
    use alloc::collections::btree_map::BTreeMap;
    use rust::thread;
    use crate::error::OPERATION_FAILED;
    use crate::page::PageMapBase;

    static mut ADDRESS_SPACES: Option<BTreeMap<PageMapBase, AddrSpace>> = None;

    pub fn init() {
        unsafe {
            ADDRESS_SPACES = match ADDRESS_SPACES {
                Some(_) => panic!("Address space map has already been initialized."),
                None => Some(BTreeMap::new()),
            }
        }

        match thread::get_root_pmap() {
            Ok(page) => {
                let mut addr_space = AddrSpace::new(page);
                addr_space.attach_thread(thread::get_tid().expect("Unable to determine tid for current thread."));

                if !register(addr_space) {
                    panic!("Unable to register the initial address space.");
                }
            },
            Err(_) => {
                error::log_error(OPERATION_FAILED, None);
                panic!("Unable to initialize the address mapper.");
            }
        }
    }

    fn addr_space_map<'a>() -> &'static BTreeMap<PageMapBase, AddrSpace> {
        unsafe {
            ADDRESS_SPACES.as_ref()
                .expect("Address space map hasn't been initialized yet.")
        }
    }

    fn addr_space_map_mut<'a>() -> &'static mut BTreeMap<PageMapBase, AddrSpace> {
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

    pub fn unregister(pmap: PageMapBase) -> Option<AddrSpace> {
        addr_space_map_mut().remove(&pmap)
    }
}

#[derive(PartialEq, Eq, Clone)]
pub struct AddressMapping {
    pub base_page: VirtualPage,
    pub region: MemoryRegion<usize>,
    pub flags: u32,
}

/*
impl PartialEq for AddressMapping {
    fn eq(&self, other: &Self) -> bool {
        self.base_page.eq(&other.base_page) &&
            self.region.eq(&other.region) &&
            self.flags == other.flags
    }
}

impl Eq for AddressMapping {}
*/

impl PartialOrd for AddressMapping {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        self.region.partial_cmp(&other.region)
    }
}

impl Ord for AddressMapping {
    fn cmp(&self, other: &Self) -> Ordering {
        self.region.cmp(&other.region)
    }
}

impl AddressMapping {
    fn new(base_page: VirtualPage, region: MemoryRegion<usize>, flags: u32) -> AddressMapping {
        AddressMapping {
            base_page,
            region,
            flags,
        }
    }
}

pub struct AddrSpace {
    root_page_map: PageMapBase,
    vaddr_map: BTreeSet<AddressMapping>,
    attached_threads: BTreeSet<Tid>,
}

impl AddrSpace {
    pub const READ_ONLY: u32 = 0x00000001;
    pub const NO_EXECUTE: u32 = 0x00000002;
    pub const COPY_ON_WRITE: u32 = 0x00000004;
    pub const GUARD: u32 = 0x00000008;
    pub const EXTEND_DOWN: u32 = 0x00000010;

    pub fn new(root_pmap: PageMapBase) -> Self {
        Self {
            root_page_map: root_pmap,
            vaddr_map: BTreeSet::new(),
            attached_threads: BTreeSet::new(),
        }
    }

    // todo: to be implemented

    pub fn allocate_stack_memory(&mut self, _stack_size: usize) -> Option<(VAddr, usize)> {
        None
    }

    pub fn root_pmap(&self) -> PageMapBase {
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

    pub fn get_mapping(&self, addr: VAddr) -> Option<&AddressMapping> {
        for map in self.vaddr_map.iter() {
            if map.region.contains(addr as usize) {
                return Some(map);
            }
        }

        None
    }

    fn contains_region(&self, region: &MemoryRegion<usize>) -> bool {
        for map in self.vaddr_map.iter() {
            if map.region.overlaps(region) {
                return true;
            }
        }

        false
    }

    /// Maps a region of memory to some block device in a particular address space.
    ///
    /// If the desired address is `None`, then pick any available address and map to it.

    pub fn map(&mut self, addr: Option<VAddr>, dev_id: &DeviceId, offset: u64, flags: u32,
               length: usize) -> Option<VAddr> {
        let new_page = VirtualPage::new(dev_id.clone(), offset, flags);

        if let Some(start_address) = addr {
            let end_address = (start_address as usize + length)
                .align(VirtualPage::SMALL_PAGE_SIZE);
            let start_address = (start_address as usize).align_trunc(VirtualPage::SMALL_PAGE_SIZE);

            if end_address <= start_address && end_address != MemoryRegion::memory_end() {
                None
            } else {
                let new_region: MemoryRegion<usize> = MemoryRegion::new(start_address, end_address);

                if self.contains_region(&new_region) {
                    None
                } else {
                    self.vaddr_map.insert(AddressMapping::new(new_page, new_region, flags));
                    Some(start_address as VAddr)
                }
            }
        } else {
            let available_regions = self.vaddr_map
                .iter()
                .fold(RegionSet::empty(), |acc, mapping| {
                    acc.union(&RegionSet::from(mapping.region.clone()))
                })
                .complement();

            for region in available_regions.into_iter() {
                if let Ok(l) = region.length() {
                    if l < length {
                        continue;
                    }
                }

                let new_region: MemoryRegion<usize> =
                    MemoryRegion::new(region.start(), region.start() + length);

                self.vaddr_map.insert(AddressMapping::new(new_page, new_region, flags));
                return Some(region.start() as VAddr);
            }

            None
        }
    }

    pub fn unmap(&mut self, start_address: VAddr, length: usize) -> bool {
        let end_address = (start_address as usize + length)
            .align(VirtualPage::SMALL_PAGE_SIZE);
        let start_address = (start_address as usize).align_trunc(VirtualPage::SMALL_PAGE_SIZE);

        if end_address > start_address {
            let addr_map = self.get_mapping(start_address as VAddr)
                .map(|m| m.clone());

            let unmapped_region: MemoryRegion<usize> = MemoryRegion::new(start_address, end_address);

            match addr_map {
                Some(mapping) => {
                    self.vaddr_map.remove(&mapping);

                    for region in mapping.region.difference(&unmapped_region).into_iter() {
                        let mut vpage = mapping.base_page.clone();

                        vpage.offset += (region.start() - mapping.region.start()) as u64;
                        self.vaddr_map.insert(AddressMapping::new(vpage, region, mapping.flags));
                    }

                    true
                },
                None => false,
            }
        } else {
            false
        }
    }
}