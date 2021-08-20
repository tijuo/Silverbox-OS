use crate::page::{PhysicalPage, VirtualPage};
use crate::error::Error;
use crate::address::PAddr;
use crate::error;
use core::prelude::v1::*;
use crate::pager::page_alloc::PhysPageAllocator;
use crate::lowlevel::phys;
use core::convert::TryFrom;

type DeviceMajor = u16;
type DeviceMinor = u16;

pub mod manager {
    use hashbrown::HashMap;
    use super::DeviceMajor;
    use crate::Tid;
    use crate::error;

    static mut DEVICE_MAP: Option<HashMap<DeviceMajor, Tid>> = None;

    pub fn init() {
        unsafe {
            DEVICE_MAP = match DEVICE_MAP {
                Some(_) => panic!("Device map has already been initialized."),
                None => Some(HashMap::new()),
            }
        }
    }

    fn device_map<'a>() -> &'static HashMap<DeviceMajor, Tid> {
        unsafe {
            DEVICE_MAP.as_ref()
                .expect("Device map hasn't been initialized yet.")
        }
    }

    fn device_map_mut<'a>() -> &'static mut HashMap<DeviceMajor, Tid> {
        unsafe {
            DEVICE_MAP.as_mut()
                .expect("Device map hasn't been initialized yet.")
        }
    }

    pub fn register(major: DeviceMajor, tid: Tid) -> Result<(), error::Error> {
        let dev_map = device_map_mut();

        if dev_map.contains_key(&major) {
            Err(error::ALREADY_REGISTERED)
        } else {
            dev_map.insert(major, tid);
            Ok(())
        }
    }

    pub fn lookup<'a, 'b>(major: &'a DeviceMajor) -> Option<&'b Tid> {
        device_map().get(major)
    }

    pub fn unregister(major: &DeviceMajor) -> Option<Tid> {
        device_map_mut().remove(major)
    }
}

pub mod mem {
    use super::DeviceMajor;
    use crate::device::DeviceMinor;

    pub const MAJOR: DeviceMajor = 1;
    pub const PMEM_MINOR: DeviceMinor = 0;
}

pub mod pseudo {
    use super::DeviceMajor;
    use crate::device::DeviceMinor;

    pub const MAJOR: DeviceMajor = 0;
    pub const NULL_MINOR: DeviceMinor = 0;
    pub const ZERO_MINOR: DeviceMinor = 1;
}

#[derive(Clone, PartialEq, Eq)]
pub struct DeviceId {
    pub major: DeviceMajor,
    pub minor: DeviceMinor,
}

impl DeviceId {
    pub fn new(id: u32) -> Self {
        Self {
            major: (id >> 16) as DeviceMajor,
            minor: (id & 0xFFFF) as DeviceMinor,
        }
    }

    pub fn new_from_tuple((major, minor): (DeviceMajor, DeviceMinor)) -> Self {
        Self { major, minor }
    }
}

impl From<u32> for DeviceId {
    fn from(id: u32) -> DeviceId {
        DeviceId::new(id)
    }
}

impl From<DeviceId> for u32 {
    fn from(dev_id: DeviceId) -> u32 {
        (dev_id.major << 16) as u32 | dev_id.minor as u32
    }
}

/// Read a block from a device into a physical page

pub fn read_page(vpage: &VirtualPage) -> Result<PhysicalPage, Error> {
    let major = vpage.device.major;
    let minor = vpage.device.minor;

    match major {
        pseudo::MAJOR => {
            match minor {
                pseudo::NULL_MINOR => Err(error::ZERO_LENGTH),   // handle a read from /dev/null
                pseudo::ZERO_MINOR => {  // handle a read from /dev/zero

                    PhysPageAllocator::alloc()
                        .ok_or_else(|| { error::OUT_OF_MEMORY })
                        .and_then(|new_frame| {
                            unsafe {
                                phys::clear_frame(&new_frame)
                                    .map(|_| PhysicalPage::from_frame(new_frame))
                            }
                        })
                },
                _ => {
                    Err(error::DEVICE_NOT_EXIST)
                }
            }
        },
        mem::MAJOR => {
            match minor {
                mem::PMEM_MINOR => {  // handle a read from /dev/pmem
                    PhysicalPage::try_from(vpage.offset as PAddr)
                        .map_err(|_| error::DEVICE_NOT_EXIST)
                },
                _ => {  // handle reads from ramdisk
                    Err(error::DEVICE_NOT_EXIST)
                }
            }
        }
        _ =>
            Err(error::NOT_IMPLEMENTED)
    }
}

/// Write a physical page to a block on a device

pub fn write_page(vpage: &VirtualPage, page: &PhysicalPage) -> Result<(), Error> {
    let major = vpage.device.major;
    let minor = vpage.device.minor;

    match major {
        pseudo::MAJOR => {
            match minor {
                pseudo::NULL_MINOR => Ok(()),   // handle a write to /dev/null
                pseudo::ZERO_MINOR => Err(error::END_OF_FILE),  // handle a write to /dev/zero,
                _ => {
                    Err(error::DEVICE_NOT_EXIST)
                }
            }
        },
        mem::MAJOR => {
            match minor {
                mem::PMEM_MINOR => {  // handle a write to /dev/pmem
                    Ok(())
                },
                _ => {  // handle writes to ramdisk
                    Err(error::DEVICE_NOT_EXIST)
                }
            }
        }
        _ =>
            Err(error::NOT_IMPLEMENTED)
    }
}
