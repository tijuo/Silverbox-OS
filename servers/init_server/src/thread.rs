use core::time::Duration;
use crate::syscall;
use alloc::string::String;

pub struct Thread {
    name: Option<String>,
    stack_size: usize,
    stack_top: * const u8,
    id: ThreadId,
}

pub struct Builder {
    name: Option<String>,
    stack_size: usize,
}

impl Builder {
    const DEFAULT_SIZE: usize = 0x400000usize;

    pub fn new() -> Self {
        Self {
            name: None,
            stack_size: Self::DEFAULT_SIZE,
        }
    }

    pub fn name(self, name: String) -> Self {
        Self {
            name: Some(name),
            stack_size: self.stack_size
        }
    }

    pub fn stack_size(self, stack_size: usize) -> Self {
        Self {
            name: self.name,
            stack_size
        }
    }
}

#[derive(Clone)]
pub struct ThreadId(u16);

impl Thread {
    pub fn unpark(&self) { }
    pub fn id(&self) -> ThreadId { self.id.clone() }
    pub fn name(&self) -> Option<&str> { self.name.as_ref().map(|s| s.as_str()) }
/*
    fn new(name: Option<String>, stack_size: usize) -> Thread {
        let (stack_memory, stack_size) = mapping::allocate_stack_memory(stack_size);

        Thread {
            name,
            stack_size,
            id: ThreadId(syscall::c_types::NULL_TID),
            stack_top: stack_memory + stack_size,
        }
    }
 */
/*
    pub fn spawn<F, T>(self, f: F) -> Result<JoinHandle<T>, error::Error>
        where  F: FnOnce() -> T + Send + 'static, T: Send + 'static {
        unsafe {
            syscall::sys_create_thread(syscall::c_types::NULL_TID,
                                       &f as * const F as * const c_void,
            core::ptr::null() as * const PAddr,
            self.stack_top as * const c_void);
        }
    }
    */
}

pub fn sleep(dur: Duration) {
    let millis = dur.as_millis().clamp(0, 0xffffffff) as u32;

    match syscall::sleep(millis) {
        Err(code) => eprintfln!("syscall::sleep() failed with code: {}", code),
        _ => (),
    };
}

pub fn yield_now() {
    match syscall::sleep(0) {
        Err(code) => eprintfln!("syscall::sleep() failed with code: {}", code),
        _ => ()
    };
}