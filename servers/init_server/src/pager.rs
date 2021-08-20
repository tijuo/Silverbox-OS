#![allow(dead_code)]

use crate::{error, Tid};
use crate::syscall;
use crate::device;
use crate::message::kernel::ExceptionMessage;
use crate::page::PhysicalPage;
use crate::mapping;
use crate::error::{OUT_OF_MEMORY, ILLEGAL_MEM_ACCESS, OPERATION_FAILED};
use alloc::string::String;
use crate::pager::page_alloc::PhysPageAllocator;
use core::ffi::c_void;
use crate::syscall::flags::map::{PM_READ_WRITE, PM_READ_ONLY};
use crate::eprintln;
use crate::syscall::c_types::ThreadInfo;
use core::convert::TryInto;
use crate::mapping::AddrSpace;

mod new_allocator {
    use crate::address::{PAddr, PSize};
    use crate::page::PhysicalPage;
    use alloc::vec::Vec;
    use core::cmp::Ordering;
    use crate::address::Align;

    #[derive(Hash, Eq, Debug)]
    pub enum Page {
        Small(PAddr),
        Large(PAddr),
        Huge(PAddr),
    }

    impl PartialEq for Page {
        fn eq(&self, other: &Self) -> bool {
            self.is_same_value(other)
                && (self.address().align_trunc(self.size()))
                == other.address().align_trunc(self.size())
        }
    }

    impl PartialOrd for Page {
        fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
            if self.is_same_value(other) {
                self.address().partial_cmp(&other.address())
            } else {
                None
            }
        }
    }

    impl Page {
        /// Creates a new small physical page frame (4 KB) at an aligned physical address.

        pub fn new_small(addr: PAddr) -> Page {
            Page::Small(addr.align_trunc(PhysicalPage::SMALL_PAGE_SIZE))
        }

        /// Creates a new large physical page frame (2 MB) at an aligned physical address.

        pub fn new_large(addr: PAddr) -> Page {
            Page::Large(addr.align_trunc(PhysicalPage::LARGE_PAGE_SIZE))
        }

        /// Creates a new huge physical page frame (1 GB) at an aligned physical address.

        pub fn new_huge(addr: PAddr) -> Page {
            Page::Huge(addr.align_trunc(PhysicalPage::HUGE_PAGE_SIZE))
        }

        /// Returns the physical address of a page.

        pub fn address(&self) -> PAddr {
            match self {
                Page::Small(addr) => *addr,
                Page::Large(addr) => *addr,
                Page::Huge(addr) => *addr,
            }
        }

        /// Returns the size, in bytes, of a page.

        pub fn size(&self) -> PSize {
            match self {
                Page::Small(_) => PhysicalPage::SMALL_PAGE_SIZE,
                Page::Large(_) => PhysicalPage::LARGE_PAGE_SIZE,
                Page::Huge(_) => PhysicalPage::HUGE_PAGE_SIZE,
            }
        }

        /// Compares two pages and returns true if they're of the same value (i.e. both are Large).
        ///
        /// # Examples
        ///
        /// ```
        /// let page1 = Page::new_small(&0x1000);
        /// let page2 = Page::new_small(&0x100000);
        /// let page3 = Page::new_large(&0x1000);
        ///
        /// assert!(page1.is_same_value(&page2));
        /// assert!(!page1.is_same_value(&page3));
        /// ```

        pub fn is_same_value(&self, other: &Self) -> bool {
            match self {
                Page::Small(_) => match other {
                    Page::Small(_) => true,
                    _ => false,
                },
                Page::Large(_) => match other {
                    Page::Large(_) => true,
                    _ => false,
                },
                Page::Huge(_) => match other {
                    Page::Huge(_) => true,
                    _ => false,
                },
            }
        }

        /// Splits a `Large` or `Huge` page into a set of `Small` pages, consuming the split page.
        ///
        /// Returns the page itself in an `Err` if it's already `Small`.
        ///
        /// # Examples
        ///
        /// ```
        /// let large_page = Page::new_large(&0x400000);
        /// let small_page = Page::new_small(&0xA0000);
        ///
        /// let result1 = large_page.split_into_small();
        /// let result2 = small_page.split_into_small();
        ///
        /// assert!(result1.is_ok());
        /// assert!(result2.is_err());
        ///
        /// assert!(result1.unwrap().contains(&0x500000));
        /// assert_eq!(result2.err().unwrap(), 0xA0000);
        /// ```

        pub fn split_into_small(self) -> Result<Vec<Page>, Page> {
            let from_size = match self {
                Page::Small(_) => return Err(self),
                Page::Large(_) => {
                    &PhysicalPage::LARGE_PAGE_SIZE
                },
                Page::Huge(_) => {
                    &PhysicalPage::HUGE_PAGE_SIZE
                }
            };

            let mut vec = Vec::new();

            let step_iter = (self.address()..self.address() + *from_size as PAddr)
                .step_by(PhysicalPage::SMALL_PAGE_SIZE as usize);

            for addr in step_iter {
                vec.push(Page::Small(addr));
            }

            Ok(vec)
        }

        /// Split a `Huge` page into a set of `Large` pages, consuming the split page.
        ///
        /// Returns the page itself in an `Err` if it's either `Large` or `Small`.
        ///
        /// # Examples
        ///
        /// ```
        /// let huge_page = Page::new_huge(&0x80000000);
        /// let large_page = Page::new_large(&0x400000);
        /// let small_page = Page::new_small(&0xA0000);
        ///
        /// let result1 = huge_page.split_into_large();
        /// let result2 = large_page.split_into_large();
        /// let result3 = small_page.split_into_large();
        ///
        /// assert!(result1.is_ok());
        /// assert!(result2.is_err());
        /// assert!(result3.is_err());
        ///
        /// assert!(result1.unwrap().contains(&0xB0040000));
        /// assert_eq!(result2.err().unwrap(), 0x400000);
        /// assert_eq!(result3.err().unwrap(), 0xA0000);
        /// ```

        pub fn split_into_large(self) -> Result<Vec<Page>, Page> {
            match self {
                Page::Huge(mut addr) => {
                    let mut vec = Vec::new();

                    for _ in 0..(PhysicalPage::HUGE_PAGE_SIZE / PhysicalPage::LARGE_PAGE_SIZE) {
                        vec.push(Page::Large(addr));
                        addr += PhysicalPage::LARGE_PAGE_SIZE;
                    }

                    Ok(vec)
                },
                _ => Err(self)
            }
        }

        pub fn coalesce_into_large(pages: Vec<Page>) -> Result<Page, Vec<Page>> {
            let all_are_small = pages
                .iter()
                .all(|page| page.size() == PhysicalPage::SMALL_PAGE_SIZE);

            if all_are_small {
                pages
                    .iter()
                    .min_by_key(|page| page.address())
                    .map(|page| page.address().align_trunc(PhysicalPage::SMALL_PAGE_SIZE))
                    .and_then(|start_addr| {
                        let mut step_iter = (start_addr..start_addr + PhysicalPage::LARGE_PAGE_SIZE as PAddr)
                            .step_by(PhysicalPage::SMALL_PAGE_SIZE as usize);

                        if step_iter.all(|addr| pages.contains(&Page::Small(addr))) {
                            Some(Page::Large(start_addr))
                        } else {
                            None
                        }
                    })
                    .ok_or_else(|| pages)
            } else {
                Err(pages)
            }
        }
    }
}

pub type DeviceId = u32;

pub mod page_alloc {
    use super::PhysicalPage;
    use crate::{PAddr, PSize};
    use crate::address::{Align, PageFrame};
    use alloc::prelude::v1::Vec;
    use crate::elf::{RawMemoryMap, RawMultibootInfo};
    use core::{mem, cmp};
    use crate::lowlevel::phys;

    static mut PAGE_ALLOCATOR: Option<PhysPageAllocator> = None;

    pub struct PhysPageAllocator {
        free_pages: Vec<PageFrame>,
        used_pages: Vec<PageFrame>
    }

    fn allocator() -> &'static PhysPageAllocator {
        unsafe {
            match PAGE_ALLOCATOR {
                Some(ref a) => a,
                None => panic!("Physical page allocator hasn't been initialized yet."),
            }
        }
    }

    fn allocator_mut() -> &'static mut PhysPageAllocator {
        unsafe {
            match PAGE_ALLOCATOR {
                Some(ref mut a) => a,
                None => panic!("Physical page allocator hasn't been initialized yet."),
            }
        }
    }

    pub fn is_allocator_ready() -> bool {
        unsafe { PAGE_ALLOCATOR.is_some() }
    }

    impl PhysPageAllocator {
        pub fn init(multiboot_info: * const RawMultibootInfo, last_free_kernel_page: PAddr) {
            let mut info_struct = RawMultibootInfo::default();

            unsafe {
                PAGE_ALLOCATOR = match PAGE_ALLOCATOR {
                    Some(_) => panic!("Physical page allocator has already been initialized."),
                    None => Some(PhysPageAllocator {
                        free_pages: Vec::new(),
                        used_pages: Vec::new()
                    })
                };
            }

            // Initialize the physical page allocator with pages

            unsafe {
                phys::peek_ptr(multiboot_info as PAddr,
                               &mut info_struct as *mut RawMultibootInfo as *mut u8,
                               mem::size_of::<RawMultibootInfo>())
                    .expect("Unable to read multiboot info.");
            }

            let mut offset = 0;
            let last_kernel_frame = PhysicalPage::new(last_free_kernel_page).unwrap().frame();

            while offset < info_struct.mmap_length {
                let mut mmap = RawMemoryMap::default();

                unsafe {
                    phys::peek_ptr((info_struct.mmap_addr + offset) as PAddr,
                                   &mut mmap as *mut RawMemoryMap as *mut u8,
                                   mem::size_of::<RawMemoryMap>())
                        .expect("Unable to read memory map.");
                }

                if mmap.map_type == RawMemoryMap::AVAILABLE {
                    let end_frame = PhysicalPage::new(mmap.base_addr + mmap.length)
                        .map_or_else(|| PhysicalPage::new(0xFFFFF000).unwrap().frame(), |page| page.frame());

                    if end_frame > last_kernel_frame {
                        match PhysicalPage::new(mmap.base_addr) {
                            Some(page) => {
                                let start_frame = cmp::max(last_kernel_frame+1, page.frame());

                                for f in (start_frame..end_frame).rev() {
                                    allocator_mut().free_pages.push(f);
                                }
                            },
                            None => (),
                        };
                    }
                }

                let mmap_size = mmap.size;
                offset += mmap.size + mem::size_of_val(&mmap_size) as u32;
            }

            allocator_mut().free_pages.shrink_to_fit();
            allocator_mut().used_pages.push(PhysicalPage::new(last_free_kernel_page).unwrap().frame());
        }

        pub fn init_test(mut start_addr: PAddr, mut total_memory: PSize) {
            start_addr = start_addr.align(PhysicalPage::SMALL_PAGE_SIZE);
            total_memory = total_memory.align_trunc(PhysicalPage::SMALL_PAGE_SIZE);

            unsafe {
                PAGE_ALLOCATOR = match PAGE_ALLOCATOR {
                    Some(_) => panic!("Physical page allocator has already been initialized."),
                    None => Some(PhysPageAllocator {
                        free_pages: Vec::new(),
                        used_pages: Vec::new()
                    })
                };
            }

            for addr in start_addr..(start_addr+total_memory) {
                allocator_mut().free_pages.push(PhysicalPage::new(addr).unwrap().frame());
            }
        }

        pub fn alloc() -> Option<PageFrame> {
            allocator_mut().free_pages
                .pop()
                .map(|page| {
                    allocator_mut().used_pages.push(page);
                    page
                })
        }

        // Note: This function cannot be used in sbrk
        pub fn alloc_pages(count: usize) -> Result<Vec<PageFrame>, usize> {
            if Self::free_count() >= count {
                let mut vec = Vec::with_capacity(count);

                for _ in 0..count {
                    match Self::alloc() {
                        Some(p) => vec.push(p),
                        None => {
                            Self::release_pages(vec.into_iter());
                            return Err(Self::free_count());
                        }
                    }
                }

                Ok(vec)
            } else {
                Err(Self::free_count())
            }
        }

        // Note: This function cannot be used in sbrk

        pub fn alloc_pages_arr(arr: &mut [PageFrame]) -> Result<&[PageFrame], usize> {
            if Self::free_count() >= arr.len() {
                for i in 0..arr.len() {
                    match Self::alloc() {
                        Some(p) => arr[i] = p,
                        None => {
                            Self::release_pages(arr.iter().map(|a| *a));
                            return Err(Self::free_count());
                        }
                    }
                }
                Ok(arr)
            } else {
                Err(Self::free_count())
            }
        }

        pub fn is_free(frame: &PageFrame) -> bool {
            allocator().free_pages.contains(frame)
        }

        pub fn is_used(frame: &PageFrame) -> bool {
            allocator().used_pages.contains(frame)
        }

        pub fn contains(frame: &PageFrame) -> bool {
            Self::is_free(frame) || Self::is_used(frame)
        }

        pub fn release(frame: PageFrame) {
            if !Self::is_used(&frame) {
                let address = PhysicalPage::from_frame(frame).as_address();

                if !Self::is_free(&frame) {
                    panic!("Attempted to release a page: 0x{:x} that's neither free nor used", address);
                } else {
                    panic!("Attempted to release a page: 0x{:x} that's already free", address);
                }
            } else {
                allocator_mut().used_pages.retain(|p| p != &frame);
                allocator_mut().free_pages.push(frame);
            }
        }

        pub fn release_pages(pages: impl Iterator<Item=PageFrame>) {
            for page in pages {
                Self::release(page)
            }
        }

        pub fn release_pages_arr(pages: &[PageFrame]) {
            for page in pages {
                Self::release(page.clone())
            }
        }

        pub fn free_count() -> usize {
            allocator().free_pages.len()
        }

        pub fn used_count() -> usize {
            allocator().used_pages.len() as usize
        }

        pub fn total_count() -> usize {
            Self::free_count() + Self::used_count()
        }
    }
}

/// The main page fault handler. Receives page fault messages from the kernel and attempts to
/// resolve the page fault by allocating memory, mapping pages, etc.

pub(crate) fn handle_page_fault(request: &ExceptionMessage) -> Result<(), (error::Error, Option<String>)> {
    //eprintln!("Handling page fault...");
    let is_read_access = request.error_code & ExceptionMessage::WRITE == 0;
    let is_kernel_access = request.error_code & ExceptionMessage::USER == 0;
    let is_not_present = request.error_code & ExceptionMessage::PRESENT == 0;

    let mut thread_info = ThreadInfo::default();
    thread_info.status = ThreadInfo::READY;

    let thread_struct = syscall::read_thread(&Tid::new(request.who.into()), ThreadInfo::REG_STATE)
        .map_err(|err| (err, None))?;

    let reg_state = thread_struct.state().unwrap();

    let addr_space = mapping::manager::lookup_tid_mut(&request.who.into()).unwrap();
    let root_pmap = addr_space.root_pmap();

    /* Is the fault address mapped in the thread's address space, but not yet committed? */

    if let Some(mapping) = addr_space.get_mapping(&request.fault_address) {
        /* Either swap the page into memory, load the page from disk into memory, or allocate a new physical page
            depending on swap status and device. */

        // The address hasn't been committed to memory

        if is_not_present {
            let mapped_frame = if let Some(_) = &mapping.page {
                let page = mapping.page.as_ref().unwrap().clone();
                match device::read_page(&page) {
                    Ok(p) => p.frame(),
                    Err(code) => return Err((OPERATION_FAILED, Some(format!("Reading block from device resulted in error {}", code))))
                }
            } else {
                match PhysPageAllocator::alloc() {
                    None => return Err((OUT_OF_MEMORY, None)),
                    Some(new_phys_page) => {
                        new_phys_page
                    }
                }
            };

            let mut flags = 0;

            flags |= if mapping.flags & AddrSpace::READ_ONLY == AddrSpace::READ_ONLY {
                PM_READ_ONLY
            } else {
                PM_READ_WRITE
            };
            /*eprintln!("Fault mapping {:p} -> {:#x} pmap: {:#x}",
                      request.fault_address, mapped_frame * 4096, root_pmap);
*/
            return unsafe {
                syscall::map(Some(root_pmap),
                             request.fault_address as *mut c_void,
                             &[mapped_frame],
                             1,
                             flags)
            }
                .and_then(|result| {
                    if result == 1 {
                        //eprintln!("Mapping ok");
                        Ok(())
                    } else {
                        //eprintln!("Unable to map page");
                        Err(OPERATION_FAILED)
                    }
                })
                .map_err(|code| (OPERATION_FAILED,
                                 Some(format!("Unable to map page. System call returned 0x{:X}", code))));
        } else if is_kernel_access && reg_state.cs != 0x10 { // Don't allow access to kernel memory
            eprintln!("Attempted to access kernel memory.");
        } else if is_read_access {     // This isn't supposed to happen
            eprintln!("Address has been committed to memory, but a read access resulted in a page fault.");
        } else {
            /* TODO: Someone wrote to a read-only page. Find out whether
                                it is allowed (COW, for example) or not and perform the
                                relevant operation. */
            if mapping.flags & AddrSpace::READ_ONLY == AddrSpace::READ_ONLY {
                eprintln!("Handling of read-only page faults isn't implemented yet");
            } else {
                //  eprintln!("No problem here...");

            }
        }
    } else {
        eprintln!("Fault address is not mapped in address space");
    }

    error::dump_state(&request.who);

    let access = if is_read_access {
        "read from"
    } else {
        "write to"
    };

    let privilege = if is_kernel_access {
        " kernel"
    } else {
        ""
    };

    Err((ILLEGAL_MEM_ACCESS,
         Some(format!("Tid {} attempted to {}{} memory at address 0x{:p}",
                      request.who.try_into().unwrap_or(0u16), access, privilege, request.fault_address))))
}

#[cfg(test)]
mod test {
    use super::new_allocator::Page;
    use crate::address::Align;
    use super::{PhysicalPage, PhysPageAllocator};
    use crate::address::{PSize, PAddr};
    use alloc::vec::Vec;

    #[test]
    fn test_allocation() {
        let (start_addr, total_mem) = (0x1000, 0x10000);
        PhysPageAllocator::init_test(start_addr, total_mem);

        assert_eq!(PhysPageAllocator::total_count(), (total_mem-start_addr) / PhysicalPage::SMALL_PAGE_SIZE);
        assert_eq!(PhysPageAllocator::total_count(), PhysPageAllocator::free_count());
        assert_eq!(PhysPageAllocator::used_count(), 0);

        let frame_option = PhysPageAllocator::alloc();

        assert!(frame_option.is_some());

        let page = PhysicalPage::from_frame(frame_option.unwrap());

        assert!(page.as_address() >= start_addr);
        assert!(page.as_address() < total_mem as PAddr);
        assert!(page.as_address().is_aligned(PhysicalPage::SMALL_PAGE_SIZE));
        assert!(!PhysPageAllocator::is_free(&page.frame()));
        assert!(PhysPageAllocator::is_used(&page.frame()));

        let page2 = page.clone();

        PhysPageAllocator::release(page.frame());

        assert!(PhysPageAllocator::is_free(&page2.frame()));
        assert!(PhysPageAllocator::is_used(&page2.frame()));
    }

    #[test]
    fn test_free_page_count() {
        let (start_addr, total_mem) = (0x0000, 0x100000);
        PhysPageAllocator::init_test(start_addr, total_mem);

        assert_eq!(PhysPageAllocator::free_count(), (total_mem-start_addr as PSize) / PhysicalPage::SMALL_PAGE_SIZE);

        let p1 = PhysPageAllocator::alloc().unwrap();
        let p2 = PhysPageAllocator::alloc().unwrap();
        let p3 = PhysPageAllocator::alloc().unwrap();

        assert_eq!(PhysPageAllocator::free_count(), ((total_mem-start_addr as PSize) / PhysicalPage::SMALL_PAGE_SIZE - 3) as PSize);

        PhysPageAllocator::release_pages_arr(&[p1, p2, p3]);

        assert_eq!(PhysPageAllocator::free_count(), (total_mem-start_addr as PSize) / PhysicalPage::SMALL_PAGE_SIZE);
    }

    #[test]
    fn test_used_page_count() {
        let (start_addr, total_mem) = (0x0000, 0x100000);
        PhysPageAllocator::init_test(start_addr, total_mem);

        assert_eq!(PhysPageAllocator::used_count(), 0);

        let p1 = PhysPageAllocator::alloc().unwrap();
        let p2 = PhysPageAllocator::alloc().unwrap();
        let p3 = PhysPageAllocator::alloc().unwrap();

        assert_eq!(PhysPageAllocator::used_count(), 3);

        PhysPageAllocator::release_pages_arr(&[p1, p2, p3]);
        assert_eq!(PhysPageAllocator::used_count(), 0);
    }

    #[test]
    fn test_empty_allocator() {
        let (start_addr, total_mem) = (0x0000, 0x0000);

        PhysPageAllocator::init_test(start_addr, total_mem);
        assert_eq!(PhysPageAllocator::free_count(), 0);
        assert!(PhysPageAllocator::alloc().is_none());
    }

    #[test]
    #[should_panic]
    fn test_double_release() {
        let (start_addr, total_mem) = (0x0000, 0x10000);
        PhysPageAllocator::init_test(start_addr, total_mem);

        let option = PhysPageAllocator::alloc();

        assert!(option.is_some());

        let page = option.unwrap();
        let cloned_page = page.clone();

        PhysPageAllocator::release(page);
        PhysPageAllocator::release(cloned_page);
    }

    #[test]
    fn test_page_sizes() {
        assert!(PhysicalPage::SMALL_PAGE_SIZE < PhysicalPage::LARGE_PAGE_SIZE);
        assert!(PhysicalPage::LARGE_PAGE_SIZE < PhysicalPage::HUGE_PAGE_SIZE);
        assert!(PhysicalPage::SMALL_PAGE_SIZE > 0);
        assert_eq!(PhysicalPage::LARGE_PAGE_SIZE % PhysicalPage::SMALL_PAGE_SIZE, 0);
        assert_eq!(PhysicalPage::HUGE_PAGE_SIZE % PhysicalPage::SMALL_PAGE_SIZE, 0);
        assert_eq!(PhysicalPage::HUGE_PAGE_SIZE % PhysicalPage::LARGE_PAGE_SIZE, 0);
    }

    #[test]
    fn test_page_eq() {
        let p1 = Page::Small(0x1000);
        let p2 = Page::Small(0x1001);
        let p3 = Page::Small(0x2000);

        let p4 = Page::Large(0x1000);
        let p5 = Page::Large(0x0000);
        let p6 = Page::Large(0x53990);
        let p7 = Page::Large(0x1000000);

        let p8 = Page::Huge(0x1000);
        let p9 = Page::Huge(0x53990);
        let p10 = Page::Huge(0xA0000000);

        assert_eq!(p1, p2);
        assert_ne!(p2, p3);
        assert_ne!(p1, p3);

        assert_eq!(p4, p5);
        assert_eq!(p4, p6);
        assert_ne!(p1, p4);
        assert_ne!(p4, p2);
        assert_ne!(p4, p7);
        assert_ne!(p6, p7);

        assert_eq!(p8, p9);
        assert_ne!(p8, p10);
    }

    #[test]
    fn test_split_into_small() {
        let p1 = Page::new_large(0x200000);

        let result = p1.split_into_small();

        assert!(result.is_ok());

        let smaller_pages = result.unwrap();

        assert_eq!(smaller_pages.len(), (PhysicalPage::LARGE_PAGE_SIZE / PhysicalPage::SMALL_PAGE_SIZE) as usize);

        for i in 0x200..0x400 {
            let addr = i * PhysicalPage::SMALL_PAGE_SIZE as PAddr;
            assert!(smaller_pages.contains(&Page::new_small(addr)))
        }

        let p2 = Page::new_huge(0);

        let result = p2.split_into_small();

        assert!(result.is_ok());

        let smaller_pages = result.unwrap();

        assert_eq!(smaller_pages.len(), (PhysicalPage::HUGE_PAGE_SIZE / PhysicalPage::SMALL_PAGE_SIZE) as usize);

        for i in 0x00..0x40000 {
            let addr = i * PhysicalPage::SMALL_PAGE_SIZE as PAddr;
            assert!(smaller_pages.contains(&Page::new_small(addr)))
        }

        let p3 = Page::new_small(0x100000);

        let result = p3.split_into_small();

        assert!(result.is_err());

        let result = result.err();

        assert!(result.is_some());

        let small_page = result.unwrap();

        assert_eq!(small_page, Page::new_small(0x100000));
    }

    #[test]
    fn test_coalesce_into_large() {
        let mut vec1 = Vec::with_capacity(1024);
        let mut vec2 = Vec::with_capacity(10);

        for addr in (0x400000..0x800000).step_by(PhysicalPage::SMALL_PAGE_SIZE as usize) {
            vec1.push(Page::new_small(addr));
        }

        for addr in (0x100000..0x10A000).step_by(PhysicalPage::SMALL_PAGE_SIZE as usize) {
            vec2.push(Page::new_small(addr));
        }

        assert_eq!(Page::coalesce_into_large(vec1), Ok(Page::new_large(0x400000)));
        assert!(Page::coalesce_into_large(vec2).is_err());
    }
}
