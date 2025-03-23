use core::ffi::c_void;
use rust::align::Align;
use crate::address::{PSize, PAddr};
use crate::error::Error;
use core::cmp;
use crate::page::PhysicalFrame;
use crate::page::VirtualPage;
use crate::phys_alloc::{self, BlockSize};
use crate::lowlevel;
use rust::syscalls::flags::mapping::PAGE_SIZED;
use rust::syscalls::SyscallError;
use core;

static mut HEAP_REGION: Option<HeapRegion> = None;
const MFAIL: * const c_void = -1isize as * const c_void;
const MIN_INCREMENT: usize = 128*1024;
const MAX_UNUSED_LEN: usize = 262144;

type FrameArray = [PAddr; 32];

extern "C" {
    static _end: *const u8;
}

pub fn init() {

}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum MapFramesError {
    BadArgument,
    Failed,
    PartiallyMapped(usize)
}

impl core::error::Error for MapFramesError {}

impl core::fmt::Display for MapFramesError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::BadArgument => f.write_str("Bad argument"),
            Self::Failed => f.write_str("Failed"),
            Self::PartiallyMapped(x) => f.write_fmt(format_args!("Partially mapped {} pages", *x))
        }
    }
}

impl From<MapFramesError> for crate::error::Error {
    fn from(value: MapFramesError) -> Self {
        match value {
            MapFramesError::BadArgument => Error::BadArgument,
            MapFramesError::Failed => Error::Failed,
            MapFramesError::PartiallyMapped(_) => Error::Incomplete
        }
    }
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

    unsafe fn map_frames(&mut self, frames: &FrameArray, frame_count: usize, use_large_pages: bool) -> Result<(), MapFramesError> {
        match lowlevel::map_frames(None,
                                  self.map_end as *mut c_void,
                                  &frames[..frame_count],
                                   if use_large_pages { PAGE_SIZED } else { 0 }) {
            Ok(pages_mapped) => {
                self.map_end += pages_mapped as usize * if use_large_pages {
                    PhysicalFrame::PSE_LARGE_PAGE_SIZE as usize
                } else {
                    PhysicalFrame::SMALL_PAGE_SIZE as usize
                };

                Ok(())
            },
            Err(SyscallError::PartiallyMapped(pages_mapped)) => {
                let mut v = self.map_end as *const u8;

                for _ in 0..pages_mapped {
                    let frame = lowlevel::unmap(None, v as *mut ())
                        .map_err(|_| MapFramesError::Failed)?;

                    v = v.wrapping_add(frame.frame_size().bytes());
                }

                Err(MapFramesError::PartiallyMapped(pages_mapped))
            },
            Err(SyscallError::InvalidArgument) => {
                eprintfln!("Unable to map memory in order to extend heap.");
                Err(MapFramesError::BadArgument)
            }
            Err(_) => {
                eprintfln!("Unable to map memory in order to extend heap.");
                Err(MapFramesError::Failed)
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

                        let page_size = if block_len as usize >= PhysicalFrame::PSE_LARGE_PAGE_SIZE {
                            use_large_pages = true;
                            PhysicalFrame::PSE_LARGE_PAGE_SIZE as PSize
                        } else {
                            use_large_pages = false;
                            PhysicalFrame::SMALL_PAGE_SIZE as PSize
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
                                    for i in 0..alloc_block_size.bytes() / page_size {
                                        lowlevel::map(None,
                                                      (self.map_end as PSize + i * page_size) as *mut c_void,
                                                      paddr + i * page_size,
                                                      if use_large_pages { PAGE_SIZED } else { 0 })
                                            .map_err(|e| {
                                                if let SyscallError::PartiallyMapped(pages_mapped) = e {
                                                    for i in 0..pages_mapped as PSize {
                                                        phys_alloc::release_phys(paddr + i * page_size,
                                                                                 if use_large_pages { BlockSize::Block4M } else { BlockSize::Block4k });
                                                    }
                                                }
                                                ()
                                            })?;
                                    }
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
                                            .map_err(|_| {
                                                // Something went wrong. Release the physical page frames
                                                // and unmap any mapped memory.

                                                for i in 0..frames_mapped as PSize {
                                                    phys_alloc::release_phys(frames[i as usize],
                                                                             if use_large_pages { BlockSize::Block4M } else { BlockSize::Block4k });
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
                                        .map_err(|_| {
                                            // Something went wrong. Release the physical page frames
                                            // and unmap any mapped memory.

                                            for i in 0..frames_mapped as PSize {
                                                phys_alloc::release_phys(frames[i as usize],
                                                                         if use_large_pages { BlockSize::Block4M } else { BlockSize::Block4k });
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
                let _unmap_start = aligned_end + MIN_INCREMENT;
                //let pages_to_unmap = (aligned_map_end - unmap_start) / VirtualPage::SMALL_PAGE_SIZE;

                // TODO: Use the array of unmapped pages from the kernel to release pages back to the page allocator

                /*
                match lowlevel::unmap(None, unmap_start, pages_to_unmap) {
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
