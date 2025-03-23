#![allow(dead_code)]

use rust::types::Tid;
use rust::{device, syscalls};
use crate::lowlevel;
use crate::mapping;
use crate::error::{self, Error};
use core::ffi::c_void;
use crate::eprintfln;
use core::convert::TryInto;
use rust::message::kernel::ExceptionMessage;
use crate::mapping::AddrSpace;
use alloc::borrow::Cow;

mod new_allocator {
    use crate::address::{PAddr, PSize};
    use crate::page::PhysicalFrame;
    use core::cmp::Ordering;
    use rust::align::Align;

    #[derive(Hash, Eq, Debug)]
    pub enum Page {
        Small(PAddr),
        Large2M(PAddr),
        Large4M(PAddr),
        Huge(PAddr),
    }

    impl PartialEq for Page {
        fn eq(&self, other: &Self) -> bool {
            self.is_same_value(other)
                && (self.address().align_trunc(self.size() as PSize))
                == other.address().align_trunc(self.size() as PSize)
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
            Page::Small(addr.align_trunc(PhysicalFrame::SMALL_PAGE_SIZE as PSize))
        }

        /// Creates a new large physical page frame (2 MB) at an aligned physical address.

        pub fn new_large_2mb(addr: PAddr) -> Page {
            Page::Large2M(addr.align_trunc(PhysicalFrame::PAE_LARGE_PAGE_SIZE as PSize))
        }

        /// Creates a new large physical page frame (4 MB) at an aligned physical address.

        pub fn new_large_4mb(addr: PAddr) -> Page {
            Page::Large4M(addr.align_trunc(PhysicalFrame::PSE_LARGE_PAGE_SIZE as PSize))
        }

        /// Creates a new huge physical page frame (1 GB) at an aligned physical address.

        pub fn new_huge(addr: PAddr) -> Page {
            Page::Huge(addr.align_trunc(PhysicalFrame::HUGE_PAGE_SIZE as PSize))
        }

        /// Returns the physical address of a page.

        pub fn address(&self) -> PAddr {
            match self {
                Page::Small(addr) => *addr,
                Page::Large2M(addr) => *addr,
                Page::Large4M(addr) => *addr,
                Page::Huge(addr) => *addr,
            }
        }

        /// Returns the size, in bytes, of a page.

        pub fn size(&self) -> usize {
            match self {
                Page::Small(_) => PhysicalFrame::SMALL_PAGE_SIZE,
                Page::Large2M(_) => PhysicalFrame::PAE_LARGE_PAGE_SIZE,
                Page::Large4M(_) => PhysicalFrame::PSE_LARGE_PAGE_SIZE,
                Page::Huge(_) => PhysicalFrame::HUGE_PAGE_SIZE,
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
                Page::Large2M(_) => match other {
                    Page::Large2M(_) => true,
                    _ => false,
                },
                Page::Large4M(_) => match other {
                    Page::Large4M(_) => true,
                    _ => false,
                },
                Page::Huge(_) => match other {
                    Page::Huge(_) => true,
                    _ => false,
                },
            }
        }

        /*
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
                Page::Large2M(_) => {
                    &PhysicalPage::PAE_LARGE_PAGE_SIZE
                },
                Page::Large4M(_) => {
                    &PhysicalPage::PSE_LARGE_PAGE_SIZE
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

        pub fn split_into_large_2mb(self) -> Result<Vec<Page>, Page> {
            match self {
                Page::Huge(mut addr) => {
                    let mut vec = Vec::new();

                    for _ in 0..(PhysicalPage::HUGE_PAGE_SIZE / PhysicalPage::PAE_LARGE_PAGE_SIZE) {
                        vec.push(Page::Large2M(addr));
                        addr += PhysicalPage::PAE_LARGE_PAGE_SIZE;
                    }

                    Ok(vec)
                },
                _ => Err(self)
            }
        }

        pub fn coalesce_into_large_2mb(pages: Vec<Page>) -> Result<Page, Vec<Page>> {
            let all_are_small = pages
                .iter()
                .all(|page| page.size() == PhysicalPage::SMALL_PAGE_SIZE);

            if all_are_small {
                pages
                    .iter()
                    .min_by_key(|page| page.address())
                    .map(|page| page.address().align_trunc(PhysicalPage::SMALL_PAGE_SIZE))
                    .and_then(|start_addr| {
                        let mut step_iter = (start_addr..start_addr + PhysicalPage::PAE_LARGE_PAGE_SIZE as PAddr)
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
        */
    }
}

pub type DeviceId = u32;

use crate::address;

/// The main page fault handler. Receives page fault messages from the kernel and attempts to
/// resolve the page fault by allocating memory, mapping pages, etc.

pub(crate) fn handle_page_fault(request: &ExceptionMessage) -> Result<(), (error::Error, Cow<'static, str>)> {
    let is_read_access = rust::is_flag_cleared!(request.error_code, ExceptionMessage::WRITE);
    let is_kernel_access = rust::is_flag_cleared!(request.error_code, ExceptionMessage::USER);
    let is_not_present = rust::is_flag_cleared!(request.error_code, ExceptionMessage::PRESENT);

    let addr_space = mapping::manager::lookup_tid_mut(&Tid::try_from(request.who)
        .expect("Faulting thread should not have a NULL tid"))
        .ok_or((Error::NotRegistered, Cow::Borrowed("Thread's address space isn't registered")))?;

    let root_pmap = addr_space.root_pmap();

    /* Is the fault address mapped in the thread's address space, but not yet committed? */

    if let Some(mapping) = addr_space.get_mapping(address::u32_into_vaddr(request.cr2)) {
        /* Either swap the page into memory, load the page from disk into memory, or allocate a new physical page
            depending on swap status and device. */

        // The address hasn't been committed to memory

        if is_not_present {
            let mapped_frame = {
                let mapping_offset = (request.cr2 as usize - mapping.region.start()) as u64;

                match mapping.base_page.device.major {
                    device::mem::MAJOR => Ok(mapping.base_page.add_offset(mapping_offset).offset),
                    device::pseudo::MAJOR if mapping.base_page.device.minor == device::pseudo::ZERO_MINOR => Ok(0),
                    _ => Err((Error::NotImplemented, Cow::Borrowed("Reading block from device resulted in error"))),
                }?
            };

            let mut flags = 0;

            flags |= if mapping.flags & AddrSpace::READ_ONLY == AddrSpace::READ_ONLY {
                syscalls::flags::mapping::READ_ONLY
            } else {
                0
            };

            /*eprintfln!("Fault mapping {:p} -> {:#x} pmap: {:#x}",
                      request.fault_address, mapped_frame, root_pmap); */

            return unsafe {
                lowlevel::map(Some(root_pmap),
                              request.cr2 as usize as *mut c_void,
                              mapped_frame,
                              flags)
            }
                .map(|_| ())
                .map_err(|_| (Error::Failed, Cow::Borrowed("Operation failed.")));
        } else if is_kernel_access && request.cs != 0x10 { // Don't allow access to kernel memory
            eprintfln!("Attempted to access kernel memory.");
        } else if is_read_access {     // This isn't supposed to happen
            eprintfln!("Address has been committed to memory, but a read access resulted in a page fault.");
        } else {
            /* TODO: Someone wrote to a read-only page. Find out whether
                                it is allowed (COW, for example) or not and perform the
                                relevant operation. */
            if mapping.flags & AddrSpace::READ_ONLY == AddrSpace::READ_ONLY {
                eprintfln!("Handling of read-only page faults isn't implemented yet");
            } else {
                //  eprintfln!("No problem here...");

            }
        }
    } else {
        eprintfln!("Fault address is not mapped in address space");
    }

    error::dump_state(&request);

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

    Err((Error::IllegalMemoryAccess,
         Cow::Owned(format!("Tid {} attempted to {}{} memory at address {:#x}",
                      request.who.try_into().unwrap_or(0u16), access, privilege, request.cr2))))
}