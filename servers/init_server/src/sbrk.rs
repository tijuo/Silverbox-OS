use core::ffi::c_void;
use crate::syscall;
use crate::address::{Align, PAddr};
use core::cmp;
use crate::page::PhysicalPage;
use crate::page::VirtualPage;
use crate::pager::page_alloc::{PhysPageAllocator, self};
use crate::lowlevel;
use crate::syscall::flags::map::PM_READ_WRITE;
use core::convert::TryFrom;

static mut HEAP_REGION: Option<HeapRegion> = None;
const MFAIL: * const c_void = -1isize as * const c_void;
const MIN_INCREMENT_PAGES: usize = 16;
const MAX_UNUSED_LEN: usize = 524288;

extern {
    static _end: *const u8;
}

pub fn init(bootstrap_paddr: PAddr) {
    HeapRegion::init(bootstrap_paddr);
}

struct HeapRegion {
    start: usize,
    end: usize,
    map_end: usize,
}

fn heap() -> &'static HeapRegion {
    unsafe {
        if HEAP_REGION.is_none() {
            panic!("Heap hasn't been initialized yet.")
        } else {
            HEAP_REGION.as_ref().unwrap()
        }
    }
}

fn heap_mut() -> &'static mut HeapRegion {
    unsafe {
        if HEAP_REGION.is_none() {
            panic!("Heap hasn't been initialized yet.")
        } else {
            HEAP_REGION.as_mut().unwrap()
        }
    }
}

impl HeapRegion {
    fn init(bootstrap_paddr: PAddr) {
        unsafe {
            let heap_begin = (&_end as *const *const u8 as usize).align(VirtualPage::SMALL_PAGE_SIZE);
            let bootstrap_page = PhysicalPage::try_from(bootstrap_paddr)
                .expect("Physical address of bootstrap page is out of range.");


            if HEAP_REGION.is_none() {
                if let Ok(pages_mapped) = syscall::map(None,
                                                       heap_begin as *mut c_void,
                                                       &[bootstrap_page.frame()],
                                                       1,
                                                       PM_READ_WRITE) {
                    if pages_mapped == 1 {
                        HEAP_REGION = Some(HeapRegion {
                            start: heap_begin,
                            end: heap_begin,
                            map_end: (heap_begin + VirtualPage::SMALL_PAGE_SIZE),
                        });

                        return;
                    }
                }
                panic!("Unable to bootstrap heap memory.");
            } else {
                panic!("Heap has already been initialized.");
            }
        }
    }

    fn len(&self) -> usize {
        cmp::max(0, self.end - self.start)
    }

    fn increment_end(&mut self, increment: isize) -> Result<(), ()> {
        if increment.is_positive() {
            let increment = increment as usize;
            let new_end = self.end + increment;

            if new_end > self.map_end {
                let n = new_end - self.map_end;
                let max_increment = if !page_alloc::is_allocator_ready() { 1 } else { MIN_INCREMENT_PAGES };

                let mut pages_to_map = cmp::max(max_increment * VirtualPage::SMALL_PAGE_SIZE,
                                                n.align(VirtualPage::SMALL_PAGE_SIZE))
                    / VirtualPage::SMALL_PAGE_SIZE;

                if (!page_alloc::is_allocator_ready() && pages_to_map > 1) || (page_alloc::is_allocator_ready() && pages_to_map > PhysPageAllocator::free_count()) {
                    return Err(())
                }

                while pages_to_map > 0 {
                    let mut frames = [0; 512];
                    let pages_to_clone = cmp::min(pages_to_map, 512);

                    for i in 0..pages_to_clone {
                        frames[i] = PhysPageAllocator::alloc().unwrap();
                    }

                    unsafe {
                        match syscall::map(None,
                                           self.map_end as *mut c_void,
                                           &frames[..pages_to_clone],
                                           pages_to_clone as i32, PM_READ_WRITE) {
                            Ok(pages_mapped) => {
                                self.map_end += pages_mapped as usize * VirtualPage::SMALL_PAGE_SIZE;

                                if pages_mapped < pages_to_clone as i32 {
                                    PhysPageAllocator::release_pages_arr(&frames[pages_mapped as usize..]);
                                    break;
                                } else {
                                    pages_to_map -= pages_mapped as usize;
                                }
                            },
                            Err(_) => {
                                lowlevel::print_debugln("Unable to map memory in order to extend heap.");
                                return Err(());
                            }
                        }
                    }
                }
            }

            self.end = new_end;
            Ok(())
        }
        else if increment.is_negative() {

            self.end = if increment == isize::MIN {
                cmp::max(self.start, (self.end + ((isize::MIN + 1) as usize)) - 1)
            } else {
                let increment = (-increment) as usize;
                cmp::max(self.start, self.end - increment)
            };

            let aligned_end = self.end.align(VirtualPage::SMALL_PAGE_SIZE);
            let aligned_map_end = self.map_end.align(VirtualPage::SMALL_PAGE_SIZE);
            let unused_len = aligned_map_end - aligned_end;

            if unused_len > MAX_UNUSED_LEN {
                let unmap_start = aligned_end + MIN_INCREMENT_PAGES * VirtualPage::SMALL_PAGE_SIZE;
                //let pages_to_unmap = (aligned_map_end - unmap_start) / VirtualPage::SMALL_PAGE_SIZE;

                // TODO: unmap would need to get an array of unmapped pages from the kernel

                /*
                match syscall::unmap(None, unmap_start, pages_to_unmap) {
                    Ok(unmap_count) => {
                        self.map_end -= unmap_count * PhysicalPage::SMALL_PAGE_SIZE;
                    },
                    Err(_) => panic!("Unable to unmap memory in order to shrink heap."),
                }

                 */
            }
            Ok(())
        }
        else {
            Ok(())
        }
    }
}

#[no_mangle]
extern "C" fn sbrk(increment: isize) -> * mut c_void {
    let prev_end;

    if unsafe { HEAP_REGION.is_none() } {
        return MFAIL as *mut c_void;
    } else {
        prev_end = heap().end as * mut c_void;
    }

    match heap_mut().increment_end(increment) {
        Ok(_) => prev_end,
        Err(_) => MFAIL as *mut c_void,
    }
}
