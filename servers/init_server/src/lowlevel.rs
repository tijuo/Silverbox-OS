use core::panic::PanicInfo;
use core::alloc::{Layout, GlobalAlloc};
use core::ffi::c_void;
use alloc::prelude::v1::ToString;

static BASE_CHARS: &'static [u8] = b"0123456789abcdefghijklmnopqrstuvwxyz";

struct InitialAllocator;

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
    use crate::address::{PAddr, PSize, PageFrame};
    use crate::address::Align;
    use core::ffi::c_void;
    use crate::syscall::flags::map::PM_READ_WRITE;
    use core::{cmp, mem};
    use core::ops::{Deref, DerefMut, Index};
    use crate::lowlevel::{GLOBAL_ALLOCATOR};
    use alloc::prelude::v1::Vec;
    use crate::pager::page_alloc::PhysPageAllocator;
    use core::alloc::{GlobalAlloc, Layout};
    use alloc::boxed::Box;

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
        static mut PAGE_MAP_AREA: [u8; 0x200000];
    }

    static mut PMAP_LOCK: i32 = 0;
    static mut PMAP_STATUS: [bool; 512] = [false; 512];

    struct PageMapArea<'a> {
        slice: &'a mut [u8],
    }

   impl<'a> PageMapArea<'a> {
        pub fn new(size: usize) -> Option<PageMapArea<'a>> {
            let size = size.align(VirtualPage::SMALL_PAGE_SIZE) / VirtualPage::SMALL_PAGE_SIZE;

            if size > 512 {
                None
            } else {
                crate::pager::page_alloc::PhysPageAllocator::alloc_pages(size)
                    .ok()
                    .and_then(|pages| {
                        let mut frames = [0; 512];

                        for (i, frame) in pages.into_iter().enumerate() {
                            frames[i] = frame;
                        }

                        unsafe {
                            Self::new_from_frames(&frames[..])
                        }
                    })
            }
        }

        pub unsafe fn new_from_frames(frames: &[PageFrame]) -> Option<PageMapArea<'a>> {
            let size = frames.len() * VirtualPage::SMALL_PAGE_SIZE;
            let mut pmap = None;

            if size > 0 && size <= PAGE_MAP_AREA.len() {
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
                        let base_addr: *mut u8 = (PAGE_MAP_AREA.as_ptr() as usize + (offset * VirtualPage::SMALL_PAGE_SIZE)) as *mut u8;

                        match syscall::map(None,
                                           base_addr as *mut c_void,
                                           &frames, len as i32, PM_READ_WRITE) {
                            Ok(pages_mapped) if pages_mapped as usize == len => {
                                for j in offset..(offset + len) {
                                    PMAP_STATUS[j] = true;
                                }

                                pmap = Some(Self {
                                    slice: core::slice::from_raw_parts_mut(base_addr, len * VirtualPage::SMALL_PAGE_SIZE)
                                });
                            },
                            Ok(pages_mapped) => {
                                let mut unmapped_frames = Vec::with_capacity(pages_mapped as usize);

                                match syscall::unmap(None,
                                                     base_addr as *mut c_void,
                                                     pages_mapped,
                                                     Some(&mut unmapped_frames[..])) {
                                    Ok(pages_unmapped) if pages_unmapped == pages_mapped => {
                                        PhysPageAllocator::release_pages(unmapped_frames.into_iter());
                                    },
                                    _ => panic!("Unable to unmap memory.")
                                }
                            },
                            Err(_) => {}
                        }

                        break;
                    }
                }

                while mutex_unlock(&mut PMAP_LOCK) != 0 {}
            }

            pmap
        }
/*
        pub fn as_ptr(&self) -> * const u8 {
            self.slice.as_ptr()
        }

        pub fn as_mut_ptr(&mut self) -> * mut u8 {
            self.slice.as_mut_ptr()
        }

        pub fn len(&self) -> usize {
            self.slice.len()
        }

 */
    }

    impl Deref for PageMapArea<'_> {
        type Target = [u8];

        fn deref(&self) -> &Self::Target {
            self.slice
        }
    }

    impl DerefMut for PageMapArea<'_> {
        fn deref_mut(&mut self) -> &mut Self::Target {
            self.slice
        }
    }

    impl Drop for PageMapArea<'_> {
        fn drop(&mut self) {
            unsafe {
                let offset = (self.slice.as_ptr() as usize - PAGE_MAP_AREA.as_ptr() as usize) / VirtualPage::SMALL_PAGE_SIZE;
                let len = self.slice.len() / VirtualPage::SMALL_PAGE_SIZE;

                while mutex_lock(&mut PMAP_LOCK) != 0 { }

                for i in offset..offset+len {
                    PMAP_STATUS[i] = false;
                }

                let mut unmapped_frames = Vec::with_capacity(len);

                match syscall::unmap(None,
                                     self.slice.as_ptr() as * mut c_void,
                                     len as i32,
                                        Some(&mut unmapped_frames[..])) {
                    Ok(pages_unmapped) if pages_unmapped as usize == len => {
                        PhysPageAllocator::release_pages(unmapped_frames.into_iter());
                    },
                    _ => panic!("Unable to unmap memory.")
                }

                while mutex_unlock(&mut PMAP_LOCK) != 0 { }
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

    pub unsafe fn try_from_phys_arr<T: Sized>(base: PAddr, index: PSize) -> Option<Box<T>> {
        try_from_phys(base + mem::size_of::<T>() as PSize * index)
    }

    /*
    impl<'a> Deref for PageMapArea<'a> {
        type Target = [u8];

        fn deref(&self) -> &Self::Target {
            self.slice
        }
    }

    impl<'a> DerefMut for PageMapArea<'a> {
        fn deref_mut(&self) -> &mut Self::Target {
            self.slice
        }
    }
    */

    impl AsRef<[u8]> for PageMapArea<'_> {
        fn as_ref(&self) -> &[u8] {
            self.slice
        }
    }

    impl AsMut<[u8]> for PageMapArea<'_> {
        fn as_mut(&mut self) -> &mut [u8] {
            self.slice
        }
    }

    impl Index<usize> for PageMapArea<'_> {
        type Output = u8;

        fn index(&self, index: usize) -> &Self::Output {
            &self.slice[index]
        }
    }

    /*
    unsafe fn page_map_area() -> * mut u8 {
        &PAGE_MAP_AREA as * const * mut u8 as usize as * mut u8
    }
*/
    pub unsafe fn clear_frame(frame: &PageFrame) -> Result<(), Error> {
        fill_frame(frame, 0)
    }

    pub unsafe fn fill_frame(frame: &PageFrame, value: u8) -> Result<(), Error> {
        PageMapArea::new_from_frames(&[*frame])
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
                let frame = match PhysicalPage::new(paddr) {
                    Some(page) => page.frame(),
                    None => return Err(bytes_read),
                };

                PageMapArea::new_from_frames(&[frame])
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
                let frame = match PhysicalPage::new(paddr) {
                    Some(page) => page.frame(),
                    None => return Err(bytes_written),
                };

                PageMapArea::new_from_frames(&[frame])
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
        /*
        print_debug("Request to allocate ");
        print_debug_u32(layout.size() as u32, 10);
        print_debugln(" bytes of memory.");
*/
        malloc(layout.size()) as *mut u8
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        /*
        print_debug("Request to deallocate ");
        print_debug_u32(_layout.size() as u32, 10);
        print_debug(" bytes of memory at address: 0x");
        print_debug_u32(ptr as usize as u32, 16);
        print_debugln("");
*/
        free(ptr as *mut c_void)
    }

    unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
        /*
        print_debug("Request to allocate ");
        print_debug_u32(layout.size() as u32, 10);
        print_debugln(" bytes of zeroed memory.");
*/
       calloc(1, layout.size()) as *mut u8
    }

    unsafe fn realloc(&self, ptr: *mut u8, _layout: Layout, new_size: usize) -> *mut u8 {
        /*
        print_debug("Request to resize the ");
        print_debug_u32(_layout.size() as u32, 10);
                        print_debug("-byte block of memory at address: 0x");
        print_debug_u32(ptr as usize as u32, 16);
        print_debug(" to ");
        print_debug_u32(new_size as u32, 10);
        print_debugln(" bytes.");
*/
        realloc(ptr as *mut c_void, new_size) as *mut u8
   }
}

#[panic_handler]
fn handle_panic(info: &PanicInfo) -> ! {
    match info.location() {
        Some(l) => {
            print_debug("'");
            print_debug(l.file());
            print_debug("' (line ");
            print_debug_u32(l.line(), 10);
            print_debug(", col. ");
            print_debug_u32(l.column(), 10);
            print_debug("): ");
        },
        None => {
        }
    }

    /*
    match info.to_string() {
        Some(s) => {
            print_debug(": ");
            print_debugln(s);
        },
        None => print_debugln("."),
    }
*/
    print_debugln(info.to_string().as_str());
    unsafe { abort() }
}

#[alloc_error_handler]
fn handle_alloc_error(layout: Layout) -> ! {
    //panic!("Unable to allocate memory of size: {}.", layout.size())
    print_debug("Unable to allocate memory of size: ");
    print_debug_u32(layout.size() as u32, 10);
    print_debugln("");
    loop {}
}


#[global_allocator]
static mut GLOBAL_ALLOCATOR: InitialAllocator = InitialAllocator;