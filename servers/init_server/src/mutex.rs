use alloc::boxed::Box;
use core::borrow::{Borrow, BorrowMut};
use core::ops::{Deref, DerefMut};
use core::cell::{Cell, RefCell, RefMut};
use crate::syscall;
use core::marker::Sync;

#[link(name="os_init", kind="static")]
extern "C" {
    fn mutex_lock(lock: *mut i32) -> i32;
}

pub enum TryLockResult {
    AlreadyLocked,
    Poisoned,
}

pub struct Mutex<T: ?Sized> {
    data: Box<RefCell<T>>,
    lock: Cell<i32>,
}

pub struct LockedResource<'a, T: 'a + ?Sized> {
    mutex: &'a Mutex<T>,
    data_ref: RefMut<'a, T>
}

impl<T> Mutex<T> {
    pub fn new(t: T) -> Mutex<T> {
        Self {
            data: Box::new(RefCell::new(t)),
            lock: Cell::new(0),
        }
    }
}

impl<T: ?Sized> Mutex<T> {
    pub fn lock(&self) -> LockedResource<'_, T> {
        unsafe {
            while mutex_lock(self.lock.as_ptr()) != 0 {
                syscall::sys_sleep(0);
            }
        }

        LockedResource {
            mutex: &self,
            data_ref: (*self.data).borrow_mut(),
        }
    }

    pub fn try_lock(&self) -> Result<LockedResource<'_, T>, TryLockResult> {
        unsafe {
            if mutex_lock(self.lock.as_ptr()) != 0 {
                return Err(TryLockResult::AlreadyLocked)
            }
        }

        Ok(LockedResource {
                mutex: &self,
                data_ref: (*self.data).borrow_mut(),
            })
    }
}

impl<'a, T: ?Sized> Deref for LockedResource<'a, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.data_ref.borrow()
    }
}

impl<'a, T: ?Sized> DerefMut for LockedResource<'a, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.data_ref.borrow_mut()
    }
}

impl<'a, T: ?Sized> Drop for LockedResource<'a, T> {
    fn drop(&mut self) {
        self.mutex.lock.set(0);
    }
}