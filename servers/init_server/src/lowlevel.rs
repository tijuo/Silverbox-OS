use core::panic::PanicInfo;
use core::alloc::{Layout, GlobalAlloc};
use core::ffi::c_void;
use alloc::string::ToString;

static BASE_CHARS: &'static [u8] = b"0123456789abcdefghijklmnopqrstuvwxyz";

#[link(name="c", kind="static")]
extern "C" {
    fn malloc(length: usize) -> *mut c_void;
    fn calloc(num_blocks: usize, block_size: usize) -> *mut c_void;
    fn free(buffer: *mut c_void);
    fn realloc(buffer: *mut c_void, new_length: usize) -> *mut c_void;
    fn abort() -> !;
}

unsafe fn io_write_byte(port: u16, data: u8) {
    asm!("out dx, al", in("al") data, in("dx") port);
}

fn put_debug_char(c: u8) {
    let debug_port = 0xE9;

    unsafe {
        io_write_byte(debug_port, c);
    }
}

pub fn print_debug_bytes(msg: &[u8]) {
    for b in msg {
        put_debug_char(*b)
    }
}

pub fn print_debug(msg: &str) {
    print_debug_bytes(msg.as_ref());
}

pub fn print_debugln(msg: &str) {
    print_debug(msg);
    print_debug("\n");
}


#[macro_export]
macro_rules! eprintln {
    () => (eprint!("\n"));

    ($($arg:tt)*) => ({
        $crate::lowlevel::print_debugln(format!($($arg)*).as_str());
    })
}

#[macro_export]
macro_rules! eprint {
    ($($arg:tt)*) => ({
        $crate::lowlevel::print_debug(format!($($arg)*).as_str());
    });
}

#[macro_export]
macro_rules! println {
    () => (eprintln!());

    ($($arg:tt)*) => ({
        eprintln!($($arg)*);
    })
}

#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => ({
        eprint!($($arg)*);
    });
}

fn u32_to_bytes(mut num: u32, base: u32) -> Option<([u8; 33], usize)> {
    if base < 2 || base > 36 {
        None
    } else {
        let mut arr = [0u8; 33];
        let mut pos = arr.len();

        while pos > 0 {
            arr[pos-1] = BASE_CHARS[(num % base) as usize];
            num /= base;

            pos -= 1;

            if num == 0 {
                break;
            }
        }

        Some((arr, pos))
    }
}

fn i32_to_bytes(num: i32, base: u32) -> Option<([u8; 33], usize)> {
    let clo: fn (([u8; 33], usize)) -> ([u8; 33], usize) = |(mut arr, pos)| {
        arr[pos - 1] = 0x2d;
        (arr, pos - 1)
    };

    match num {
        x if x == i32::MIN => {
            u32_to_bytes(0x80000000u32, base)
                .map(clo)
        },
        x if x < 0 => {
            u32_to_bytes((-x) as u32, base)
                .map(clo)
        },
        x => u32_to_bytes(x as u32, base),
    }
}

fn print_debug_num(bytes: Option<([u8; 33], usize)>) {
    match bytes {
        Some((arr, pos)) => print_debug_bytes(&arr[pos..]),
        None => print_debug("0"),
    }
}

pub fn print_debug_u32(num: u32, base: u32) {
    print_debug_num(u32_to_bytes(num, base));
}

pub fn print_debug_i32(num: i32, base: u32) {
    print_debug_num(i32_to_bytes(num, base));
}

pub mod phys {
    use crate::syscall;
    use crate::page::{PhysicalPage, VirtualPage};
    use crate::error::{self, Error};
    use crate::address::{PAddr, PSize};
    use crate::address::Align;
    use core::ffi::c_void;
    use crate::syscall::flags::map::READ_WRITE;
    use core::{cmp, mem};
    use core::ops::{Deref, DerefMut, Index};
    use crate::lowlevel::{GLOBAL_ALLOCATOR};
    use crate::phys_alloc::{self, BlockSize};
    use core::alloc::{GlobalAlloc, Layout};
    use alloc::boxed::Box;
    use alloc::vec::Vec;

    #[link(name="c", kind="static")]
    extern "C" {
        fn memcpy(dest: *mut c_void, src: *const c_void, length: usize) -> *mut c_void;
    }

    #[link(name="os_init", kind="static")]
    extern "C" {
        fn mutex_lock(lock: *mut i32) -> i32;
        fn mutex_unlock(lock: *mut i32) -> i32;
    }

    extern "C" {
        static mut PAGE_MAP_AREA: [u8; 0x400000];
    }

    static mut PMAP_LOCK: i32 = 0;
    static mut PMAP_STATUS: [bool; 1024] = [false; 1024];

    pub struct PageMapArea {
        slice: &'static mut [u8],
        has_frames_mapped: bool,
    }

   impl PageMapArea {
       pub fn new(size: usize) -> Option<PageMapArea> {
           let size = size.align(VirtualPage::SMALL_PAGE_SIZE);
           let num_pages = size / VirtualPage::SMALL_PAGE_SIZE;

           if size > BlockSize::Block4M.bytes() as usize {
               None
           } else {
               let mut frames = Vec::new();

               for i in 0..num_pages {
                   match phys_alloc::alloc_phys(BlockSize::Block4k) {
                       Ok(addr) => frames.push(addr.0),
                       Err(_) => {
                           for j in 0..i {
                               phys_alloc::release_phys(frames[j], BlockSize::Block4k);
                               return None;
                           }
                       }
                   }
               }

               unsafe {
                   Self::new_from_frames(&frames[..num_pages])
                       .map(|mut pmap| {
                           pmap.has_frames_mapped = true;
                           pmap
                       })
               }
           }
       }

       pub unsafe fn new_from_addr(addr: PAddr) -> Option<PageMapArea> {
           PageMapArea::new_from_frames(&[addr])
       }

       fn acquire_buffer(size: usize) -> Option<&'static mut [u8]> {
           let mut buffer_option = None;

           unsafe {
               if size <= PAGE_MAP_AREA.len() {
                   while mutex_lock(&mut PMAP_LOCK) != 0 {}

                   let mut len = 0;
                   let mut offset = 0;

                   for i in 0..PMAP_STATUS.len() {
                       if !PMAP_STATUS[i] {
                           len += 1;
                       } else {
                           len = 0;
                           offset = i + 1;
                           continue;
                       };

                       if len * VirtualPage::SMALL_PAGE_SIZE >= size {
                           buffer_option = Some(&mut PAGE_MAP_AREA[offset*VirtualPage::SMALL_PAGE_SIZE..(offset+len)*VirtualPage::SMALL_PAGE_SIZE]);
                           break;
                       }
                   }

                   while mutex_unlock(&mut PMAP_LOCK) != 0 {}
               }
           }
           buffer_option
       }

       fn release_buffer(buffer: &mut [u8]) {
           unsafe {
               while mutex_lock(&mut PMAP_LOCK) != 0 {}

               let buffer_start = (buffer.as_ptr() as usize) - (PAGE_MAP_AREA.as_ptr() as usize);
               let buffer_end = buffer_start + buffer.len();

               for i in (buffer_start / VirtualPage::SMALL_PAGE_SIZE)..(buffer_end / VirtualPage::SMALL_PAGE_SIZE) {
                   PMAP_STATUS[i] = false;
               }

               while mutex_unlock(&mut PMAP_LOCK) != 0 {}
           }
       }

       pub unsafe fn new_from_frames(frames: &[PAddr]) -> Option<PageMapArea> {
            let size = frames.len() * VirtualPage::SMALL_PAGE_SIZE;

            if size > 0 && size <= PAGE_MAP_AREA.len() {
                Self::acquire_buffer(size)
                    .and_then(|base_ref| {
                        match syscall::map_frames(None,
                                                  base_ref.as_ptr() as *mut c_void,
                                                  &frames, frames.len() as i32, READ_WRITE) {
                            Ok(pages_mapped) if pages_mapped == frames.len() as i32 => {
                                Some(Self {
                                    slice: base_ref,
                                    has_frames_mapped: false,
                                })
                            },
                            Ok(pages_mapped) => {
                                super::print_debug("new_from_frames failed. only mapped ");
                                super::print_debug_i32(pages_mapped, 10);
                                super::print_debug(" pages instead of ");
                                super::print_debug_u32(frames.len() as u32, 10);
                                super::print_debugln(".");

                                match syscall::unmap(None,
                                                     base_ref.as_ptr() as *mut c_void,
                                                     pages_mapped,
                                                     None) {
                                    Ok(pages_unmapped) if pages_unmapped == pages_mapped => None,
                                    _ => panic!("Unable to unmap memory."),
                                }
                            },
                            Err(_) => None
                        }
                    })
            } else {
                None
            }
        }
    }

    impl Deref for PageMapArea {
        type Target = [u8];

        fn deref(&self) -> &Self::Target {
            self.slice
        }
    }

    impl DerefMut for PageMapArea {
        fn deref_mut(&mut self) -> &mut Self::Target {
            self.slice
        }
    }

    impl Clone for PageMapArea {
        fn clone(&self) -> Self {
            match PageMapArea::new(self.slice.len()) {
                Some(pmap) => {
                    pmap.slice.copy_from_slice(&self.slice);

                    pmap
                },
                None => panic!("Unable to clone page map."),
            }
        }
    }

    impl Drop for PageMapArea {
        fn drop(&mut self) {
            Self::release_buffer(self.slice.as_mut());

            unsafe {
                let len = self.slice.len() / VirtualPage::SMALL_PAGE_SIZE;

                let mut unmapped_frame_arr = Vec::with_capacity(len);
                let mut unmapped_frame_entries = Vec::with_capacity(len);

                match syscall::unmap(None,
                                     self.slice.as_ptr() as *mut c_void,
                                     len as i32,
                                     Some((&mut unmapped_frame_arr, &mut unmapped_frame_entries))) {
                    Ok(pages_unmapped) => {
                        if self.has_frames_mapped {
                            (0..pages_unmapped as usize)
                                .for_each(|frame| {
                                    let is_large_page = unmapped_frame_entries[frame] & syscall::flags::unmap::WAS_LARGE_PAGE != 0;

                                    phys_alloc::release_phys(unmapped_frame_arr[frame], if is_large_page { BlockSize::Block4M } else { BlockSize::Block4k });
                                })
                        }
                    },
                    _ => panic!("Unable to unmap memory.")
                }
            }
        }
    }

    pub unsafe fn try_from_phys<T: Sized>(paddr: PAddr) -> Option<Box<T>> {
        let result = Layout::from_size_align(mem::size_of::<T>(), 4);

        if let Ok(layout) = result {
            let ptr: *mut T = GLOBAL_ALLOCATOR.alloc(layout).cast();

            if ptr.is_null() {
                return None
            } else {
                peek_ptr(paddr, ptr, mem::size_of::<T>())
                    .ok()
                    .map(move |_| unsafe { Box::from_raw(ptr) })
            }
        } else {
            None
        }
    }

    pub unsafe fn try_read_from_phys<T: Sized>(paddr: PAddr, t: &mut T) -> bool {
        peek_ptr(paddr, t, mem::size_of::<T>()).is_ok()
    }

    pub unsafe fn try_from_phys_arr<T: Sized>(base: PAddr, index: PSize) -> Option<Box<T>> {
        try_from_phys(base + mem::size_of::<T>() as PSize * index)
    }

    impl AsRef<[u8]> for PageMapArea {
        fn as_ref(&self) -> &[u8] {
            self.slice
        }
    }

    impl AsMut<[u8]> for PageMapArea {
        fn as_mut(&mut self) -> &mut [u8] {
            self.slice
        }
    }

    impl Index<usize> for PageMapArea {
        type Output = u8;

        fn index(&self, index: usize) -> &Self::Output {
            &self.slice[index]
        }
    }

    pub unsafe fn clear_frame(frame: PAddr) -> Result<(), Error> {
        fill_frame(frame, 0)
    }

    pub unsafe fn fill_frame(addr: PAddr, value: u8) -> Result<(), Error> {
        PageMapArea::new_from_addr(addr)
            .ok_or_else(|| error::OPERATION_FAILED)
            .map(|mut pmap_area| pmap_area.as_mut().fill(value) )
    }

    pub fn peek(paddr: PAddr, buffer: &mut [u8]) -> Result<(), usize> {
        unsafe { read_phys_mem(paddr, buffer.as_mut_ptr(), buffer.len()) }
    }

    pub unsafe fn poke(paddr: PAddr, buffer: &[u8]) -> Result<(), usize> {
        write_phys_mem(paddr, buffer.as_ptr(), buffer.len())
    }

    pub unsafe fn poke_ptr<T>(paddr: PAddr, buffer: *mut T, bytes: usize) -> Result<(), usize> {
        write_phys_mem(paddr, buffer.cast(), bytes)
    }

    pub unsafe fn peek_ptr<T>(paddr: PAddr, buffer: *mut T, bytes: usize) -> Result<(), usize> {
        read_phys_mem(paddr, buffer.cast(), bytes)
    }

    unsafe fn read_phys_mem(mut paddr: PAddr, buffer: *mut u8, mut length: usize) -> Result<(), usize> {
        if buffer.is_null() {
            Err(0)
        } else {
            let mut bytes_read = 0;

            while length > 0 {
                let phys_offset = (paddr - paddr.align_trunc(PhysicalPage::SMALL_PAGE_SIZE)) as usize;
                let bytes = cmp::min(VirtualPage::SMALL_PAGE_SIZE - phys_offset, length);

                PageMapArea::new_from_addr(paddr)
                    .ok_or_else(|| bytes_read)
                    .map(|mapping_area| {
                        memcpy(buffer.wrapping_add(bytes_read).cast(),
                               mapping_area.as_ptr().wrapping_add(phys_offset).cast(),
                               bytes)
                    })?;

                paddr += bytes as PSize;
                bytes_read += bytes;
                length -= bytes;
            }

            Ok(())
        }
    }

    unsafe fn write_phys_mem(mut paddr: PAddr, buffer: *const u8, mut length: usize) -> Result<(), usize> {
        if buffer.is_null() {
            Err(0)
        } else {
            let mut bytes_written = 0;

            while length > 0 {
                let phys_offset = (paddr - paddr.align_trunc(PhysicalPage::SMALL_PAGE_SIZE)) as usize;
                let bytes = cmp::min(VirtualPage::SMALL_PAGE_SIZE - phys_offset, length);

                PageMapArea::new_from_frames(&[paddr])
                    .ok_or_else(|| bytes_written)
                    .map(|mut mapping_area| {
                        memcpy(mapping_area.as_mut_ptr().wrapping_add(phys_offset).cast(),
                               buffer.wrapping_add(bytes_written).cast(),
                               bytes)
                    })?;

                paddr += bytes as PSize;
                bytes_written += bytes;
                length -= bytes;
            }

            Ok(())
        }
    }
}

unsafe impl GlobalAlloc for InitialAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        malloc(layout.size()) as *mut u8
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        free(ptr as *mut c_void)
    }

    unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
       calloc(1, layout.size()) as *mut u8
    }

    unsafe fn realloc(&self, ptr: *mut u8, _layout: Layout, new_size: usize) -> *mut u8 {
        realloc(ptr as *mut c_void, new_size) as *mut u8
   }
}

#[panic_handler]
fn handle_panic(info: &PanicInfo) -> ! {
    print_debug("Panic occurred ");

    match info.location() {
        Some(l) => {
            print_debug("in file '");
            print_debug(l.file());
            print_debug("' (line ");
            print_debug_u32(l.line(), 10);
            print_debug(", col. ");
            print_debug_u32(l.column(), 10);
            print_debug(")");

            if info.message().is_some() {
                print_debug(":");
            }

            print_debug(" ");
        },
        None => {
        }
    }

    if crate::phys_alloc::is_bootstrap_ready() || crate::phys_alloc::is_allocator_ready() {
        print_debugln(info.to_string().as_str())
    } else {
        print_debugln(info.message().and_then(|msg| msg.as_str()).unwrap_or_default());
    }

    unsafe { abort() }
}

#[alloc_error_handler]
fn handle_alloc_error(layout: Layout) -> ! {
    //panic!("Unable to allocate memory of size: {}.", layout.size());

    print_debug("Unable to allocate memory of size: ");
    print_debug_u32(layout.size() as u32, 10);
    print_debugln("");
    loop {}
}

struct InitialAllocator;

#[global_allocator]
static mut GLOBAL_ALLOCATOR: InitialAllocator = InitialAllocator;