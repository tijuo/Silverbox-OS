type DeviceMajor = u16;
type DeviceMinor = u16;

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
        ((dev_id.major as u32) << 16) | dev_id.minor as u32
    }
}

impl From<i32> for DeviceId {
    fn from(id: i32) -> DeviceId {
        DeviceId::new(id as u32)
    }
}

impl From<DeviceId> for i32 {
    fn from(dev_id: DeviceId) -> i32 {
        (((dev_id.major as u32) << 16) | dev_id.minor as u32) as i32
    }
}