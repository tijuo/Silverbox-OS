use core::ffi::c_void;
use crate::syscall;
use crate::address::{Align, PSize, PAddr};
use core::cmp;
use crate::page::PhysicalPage;
use crate::page::VirtualPage;
use crate::phys_alloc::{self, BlockSize};
use crate::lowlevel;
use crate::syscall::flags::map::{READ_WRITE, PM_LARGE_PAGE};

static mut HEAP_REGION: Option<HeapRegion> = None;
const MFAIL: * const c_void = -1isize as * const c_void;
const MIN_INCREMENT: usize = 128*1024;
const MAX_UNUSED_LEN: usize = 262144;

type FrameArray = [PAddr; 32];

extern {
    static _end: *const u8;
}

pub fn init() {

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
    fn init() {
        unsafe {
            let heap_begin = (&_end as *const *const u8 as usize).align(VirtualPage::SMALL_PAGE_SIZE);

            if HEAP_REGION.is_none() {
                HEAP_REGION = Some(HeapRegion {
                    start: heap_begin,
                    end: heap_begin,
                    map_end: heap_begin,
                });
            } else {
                panic!("Heap has already been initialized.");
            }
        }
    }

    fn len(&self) -> usize {
        cmp::max(0, self.end - self.start)
    }

    unsafe fn map_frames(&mut self, frames: &FrameArray, frame_count: usize, use_large_pages: bool) -> Result<(), i32> {
        match syscall::map_frames(None,
                                  self.map_end as *mut c_void,
                                  &frames[..frame_count],
                                  frame_count as i32, READ_WRITE | if use_large_pages { PM_LARGE_PAGE } else { 0 }) {
            Ok(pages_mapped) if pages_mapped == frame_count as i32 => {
                self.map_end += pages_mapped as usize * if use_large_pages {
                    PhysicalPage::PSE_LARGE_PAGE_SIZE as usize
                } else {
                    PhysicalPage::SMALL_PAGE_SIZE as usize
                };

                Ok(())
            },
            Ok(pages_mapped) => {
                Err(pages_mapped)
            },
            Err(e) => {
                lowlevel::print_debugln("Unable to map memory in order to extend heap.");
                Err(e)
            }
        }
    }

    fn increment_end(&mut self, increment: isize) -> Result<usize, ()> {
        let result = Ok(self.end);

        if increment.is_positive() {
            let increment = increment as usize;
            let new_end = self.end + increment;

            if new_end > self.map_end {
                let mut map_bytes: usize = cmp::max(
                    (new_end - self.map_end).align(VirtualPage::SMALL_PAGE_SIZE),
                    MIN_INCREMENT);

                let mut frames = [0; 32];
                let mut frames_mapped = 0;

                'map_loop: while map_bytes > 0 {
                    // Find the largest set of blocks that are less than or equal to the increment size
                    // If none are available then move to the next smallest block

                    for level in (0..BlockSize::total_sizes()).rev() {
                        let block_size = BlockSize::new(level);
                        let block_len = block_size.bytes();
                        let use_large_pages;

                        // Divide each block into either large pages (4M for PSE) or small pages
                        // (the largest one that also fits in the block)

                        let page_size = if block_len >= PhysicalPage::PSE_LARGE_PAGE_SIZE {
                            use_large_pages = true;
                            PhysicalPage::PSE_LARGE_PAGE_SIZE
                        } else {
                            use_large_pages = false;
                            PhysicalPage::SMALL_PAGE_SIZE
                        };

                        while map_bytes as u64 >= block_len {
                            // Allocate a block of physical memory, then map it to the end of the
                            // heap.

                            let (paddr, alloc_block_size) = phys_alloc::alloc_phys(block_size)
                                .map_err(|_| ())?;

                            // If a block isn't equal to the page size, then map each block
                            // one-by-one. Otherwise, collect a batch of pages and use
                            // the PM_ARRAY flag to minimize sys_map() calls.

                            if alloc_block_size.bytes() != page_size {
                                unsafe {
                                    syscall::map(None,
                                                 self.map_end as *mut c_void,
                                                 paddr,
                                                 (alloc_block_size.bytes() / page_size) as i32,
                                                 READ_WRITE
                                                     | (if use_large_pages { PM_LARGE_PAGE } else { 0 }))
                                        .map_err(|pages_mapped| {
                                            if pages_mapped > 0 {
                                                for i in 0..pages_mapped as PSize {
                                                    phys_alloc::release_phys(paddr + i * page_size,
                                                                             if use_large_pages { BlockSize::Block4M } else { BlockSize::Block4k });
                                                }
                                            }
                                            ()
                                        })?;
                                }
                                self.map_end += alloc_block_size.bytes() as usize;
                            } else {
                                let frame_count = alloc_block_size.bytes() / page_size;

                                // Before we map this block, if the frame array doesn't have
                                // enough space to hold the physical frames corresponding to this
                                // block, then call sys_map() to flush the array first.

                                if frame_count as usize > frames.len() - frames_mapped as usize {
                                    unsafe {
                                        self.map_frames(&frames, frames_mapped as usize, use_large_pages)
                                            .map_err(|pages_mapped| {
                                                // Something went wrong. Release the physical page frames
                                                // and unmap any mapped memory.

                                                for i in 0..frames_mapped as PSize {
                                                    phys_alloc::release_phys(frames[i as usize],
                                                                             if use_large_pages { BlockSize::Block4M } else { BlockSize::Block4k });
                                                }

                                                if pages_mapped > 0 {
                                                    (0..pages_mapped)
                                                        .for_each(|_| {
                                                            syscall::unmap(None,
                                                                           self.map_end as *const c_void,
                                                                           pages_mapped,
                                                                           None);
                                                        });
                                                }

                                                ()
                                            })?
                                    }

                                    frames_mapped = 0;
                                }

                                for i in 0..((frame_count) as usize).min(32) {
                                    frames[frames_mapped + i] = paddr + i as PSize * page_size;
                                }

                                self.map_end += frames_mapped*alloc_block_size.bytes() as usize;

                                frames_mapped += frame_count as usize;

                                // Now map the frames corresponding to this block.

                                unsafe {
                                    self.map_frames(&frames, frames_mapped as usize, use_large_pages)
                                        .map_err(|pages_mapped| {
                                            // Something went wrong. Release the physical page frames
                                            // and unmap any mapped memory.

                                            for i in 0..frames_mapped as PSize {
                                                phys_alloc::release_phys(frames[i as usize],
                                                                         if use_large_pages { BlockSize::Block4M } else { BlockSize::Block4k });
                                            }

                                            if pages_mapped > 0 {
                                                (0..pages_mapped).for_each(|_| {
                                                        syscall::unmap(None, self.map_end as *const c_void, pages_mapped, None);
                                                    });
                                            }

                                            ()
                                        })?
                                }

                                frames_mapped = 0;
                            }

                            if alloc_block_size.bytes() >= map_bytes as u64 {
                                break 'map_loop;
                            } else {
                                map_bytes -= alloc_block_size.bytes() as usize;
                            }
                        }
                    }
                }
            }

            self.end = new_end;
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

            // TODO: Reclaim memory by unmapping unused pages if it exceeds a particular count

            if unused_len > MAX_UNUSED_LEN {
                let unmap_start = aligned_end + MIN_INCREMENT;
                //let pages_to_unmap = (aligned_map_end - unmap_start) / VirtualPage::SMALL_PAGE_SIZE;

                // TODO: Use the array of unmapped pages from the kernel to release pages back to the page allocator

                /*
                match syscall::unmap(None, unmap_start, pages_to_unmap) {
                    Ok(unmap_count) => {
                        self.map_end -= unmap_count * PhysicalPage::SMALL_PAGE_SIZE;
                    },
                    Err(_) => panic!("Unable to unmap memory in order to shrink heap."),
                }
                */
            }
        }

        result
    }
}

#[no_mangle]
extern "C" fn sbrk(increment: isize) -> * mut c_void {
    unsafe {
        if HEAP_REGION.is_none() {
            HeapRegion::init();
            return MFAIL as *mut c_void;
        }
    }

    match heap_mut().increment_end(increment) {
        Ok(prev_end) => prev_end as *mut c_void,
        Err(_) => MFAIL as *mut c_void,
    }
}
