use core::cell::UnsafeCell;
use core::sync::atomic::AtomicBool;
use core::sync::atomic::Ordering;
use core::ops::{Deref, DerefMut};

pub struct Mutex<T: ?Sized> {
    locked: AtomicBool,
    inner: UnsafeCell<MutexInner<T>>
}

pub struct MutexInner<T: ?Sized> {
    data: T,
}

pub struct MutexGuard<'a, T: ?Sized> {
    mutex_ref: &'a Mutex<T>
}

impl<T> Mutex<T> {
    pub const fn new(data: T) -> Self {
        Self {
            locked: AtomicBool::new(false),
            inner: UnsafeCell::new(MutexInner { data }),
        }
    }

    pub fn lock(&self) -> MutexGuard<'_, T> {
        while self.locked
            .compare_exchange(false, true, Ordering::Acquire, Ordering::Relaxed)
            .is_err() {}
        MutexGuard { mutex_ref: &self }
    }
}

impl<'a, T> Deref for MutexGuard<'a, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        unsafe { &(*self.mutex_ref.inner.get()).data }
    }
}

impl<'a, T> DerefMut for MutexGuard<'a, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        unsafe { &mut (*self.mutex_ref.inner.get()).data  }
    }
}

impl<'a, T: ?Sized> Drop for MutexGuard<'a, T> {
    fn drop(&mut self) {
        self.mutex_ref.locked.store(false, Ordering::Release);
    }
}

unsafe impl<T: ?Sized> Sync for Mutex<T> {}