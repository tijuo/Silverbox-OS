#![allow(dead_code)]

use crate::Tid;
use crate::syscall::status;
use crate::syscall::c_types::{CTid, NULL_TID};
use crate::syscall;
use crate::error;
use alloc::prelude::v1::Vec;
use core::ffi::c_void;
use core::{mem, result};
use alloc::boxed::Box;
use core::convert::{TryInto, TryFrom};
use core::ptr;

type Result<T> = core::result::Result<T, error::Error>;
pub type BlankMessage = Message<()>;

#[derive(Clone)]
#[repr(C)]
pub struct RawMessage {
    subject: i32,
    sender: CTid,
    recipient: CTid,
    buffer: *mut c_void,
    buffer_len: usize,
    bytes_transferred: usize,
    flags: i32,
}

impl RawMessage {
    pub const ANY_SENDER: CTid = 0;
    pub const KERNEL_TID: CTid = 0;
    pub const RESPONSE_OK: i32 = 0;
    pub const RESPONSE_FAIL: i32 = 1;

    pub const MSG_NOBLOCK: i32 = 1;
    pub const MSG_SYSTEM: i32 = 2;
    pub const MSG_CALL: i32 = 4;
    pub const MSG_KERNEL: i32 = -0x80000000i32;

    /*
    pub fn new(subject: i32, sender: Option<CTid>, recipient: Option<CTid>,
               data: Option<&[u8]>, flags: i32) -> RawMessage {
        let buffer = data.unwrap_or(&[])
        RawMessage {
            subject,
            sender: sender.unwrap_or(0),
            recipient: recipient.unwrap_or(0),
            buffer: .as_ptr_mut(),
            buffer_len: ,
        }
    }
*/
    pub fn new_blank() -> Self {
        RawMessage {
            subject: 0,
            sender: NULL_TID,
            recipient: NULL_TID,
            buffer: ptr::null_mut(),
            buffer_len: 0,
            bytes_transferred: 0,
            flags: 0,
        }
    }

    pub fn is_flags_set(&self, flags: i32) -> bool {
        self.flags & flags == flags
    }

    pub fn flags(&self) -> i32 {
        self.flags
    }

    pub fn subject(&self) -> i32 {
        self.subject
    }

    pub fn recipient(&self) -> CTid {
        self.recipient
    }

    pub fn sender(&self) -> CTid {
        self.sender
    }

    pub fn bytes_transferred(&self) -> usize {
        self.bytes_transferred
    }
}

#[derive(Clone)]
pub struct Message<T: Sized> {
    pub subject: i32,
    pub sender: Tid,
    pub recipient: Tid,
    data: Option<Box<T>>,
    bytes_transferred: Option<usize>,
    pub flags: i32,
}

impl<T: Sized> Message<T> {
    pub fn new(subject: i32, data: Option<Box<T>>, flags: i32) -> Self {
        Self {
            subject,
            sender: Tid::null(),
            recipient: Tid::null(),
            data,
            bytes_transferred: None,
            flags,
        }
    }

    pub fn new_blank() -> Self {
        Self::new(0, None, 0)
    }

    pub fn data(&self) -> Option<&T> {
        self.data.as_ref().map(|r| &**r)
    }

    pub fn data_mut(&mut self) -> Option<&mut T> {
        self.data.as_mut().map(|r| &mut **r)
    }

    pub fn bytes_transferred(&self) -> Option<usize> {
        self.bytes_transferred.clone()
    }

    pub fn take(&mut self) -> Option<Box<T>> {
        self.data.take()
    }

    pub fn raw_message(&self) -> RawMessage {
        let len = match self.data() {
            Some(r) => mem::size_of_val(r),
            None => 0
        };

        RawMessage {
            subject: self.subject,
            sender: self.sender.into(),
            recipient: self.recipient.into(),
            buffer: match self.data() {
                Some(r) => r as *const T as *mut c_void,
                None => ptr::null_mut(),
            },
            buffer_len: len,
            bytes_transferred: 0,
            flags: self.flags
        }
    }

    pub fn send(&mut self, recipient: &Tid, subject: i32, flags: i32) -> Result<usize> {
        self.recipient = recipient.clone();
        self.subject = subject;
        self.flags = flags;

        let mut raw_msg = self.raw_message();

        match unsafe { syscall::sys_send(&mut raw_msg as *mut RawMessage) } {
            status::OK => {
                self.sender = Tid::new(raw_msg.sender);
                Ok(raw_msg.bytes_transferred)
            },
            r => Err(r),
        }
    }

    pub fn call<U>(&mut self, recipient: &Tid, subject: i32, flags: i32) -> Result<(Message<U>, usize)> {
        self.recipient = recipient.clone();
        self.subject = subject;
        self.flags = flags;

        let u_size = mem::size_of::<T>();
        let mut v: Vec<u8> = vec![0; u_size];
        let mut send_msg = self.raw_message();
        let mut recv_msg = RawMessage {
            subject,
            sender: Tid::null_ctid(),
            recipient: Tid::null_ctid(),
            buffer: v.as_mut_ptr() as *mut c_void,
            buffer_len: u_size,
            bytes_transferred: 0,
            flags,
        };

        match unsafe { syscall::sys_call(&mut send_msg as *mut RawMessage,
                                         &mut recv_msg as *mut RawMessage) } {
            status::OK => {
                let bytes_transferred = recv_msg.bytes_transferred;
                recv_msg
                    .try_into()
                    .map_err(|_| error::PARSE_ERROR)
                    .map(|m| (m, bytes_transferred))
            },
            r => Err(r),
        }
    }
}

impl<T> TryFrom<RawMessage> for Message<T> {
    type Error = i32;

    fn try_from(value: RawMessage) -> result::Result<Self, Self::Error> {
        if value.buffer_len != mem::size_of::<T>() || value.buffer.is_null() && value.buffer_len != 0 {
            result::Result::Err(error::PARSE_ERROR)
        } else {
            result::Result::Ok(Message {
                subject: value.subject,
                sender: Tid::new(value.sender),
                recipient: Tid::new(value.recipient),
                data: if value.buffer.is_null() {
                    None
                } else {
                    Some(Box::new(unsafe { ptr::read(value.buffer as *const T) }))
                },
                bytes_transferred: Some(value.bytes_transferred),
                flags: value.flags,
            })
        }
    }
}

pub fn send<T: Sized>(recipient: &Tid, message: &mut Message<T>, flags: i32) -> Result<usize> {
    message.send(recipient, message.subject, flags)
}

pub fn call<S, R>(recipient: &Tid, send_msg: &mut Message<S>, flags: i32) -> Result<(Message<R>, usize)>
    where S: Sized, R: Sized {
    send_msg.call(recipient, send_msg.subject, flags)
}

pub fn receive<T: Sized>(sender: &Tid, flags: i32) -> Result<(Message<T>, usize)> {
    let t_size = mem::size_of::<T>();
    let mut v: Vec<u8> = vec![0; t_size];
    let mut raw_msg = RawMessage {
        subject: 0,
        sender: sender.into(),
        recipient: Tid::null_ctid(),
        buffer: v.as_mut_ptr() as *mut c_void,
        buffer_len: t_size,
        bytes_transferred: 0,
        flags
    };

    match unsafe { syscall::sys_receive(&mut raw_msg as *mut RawMessage) } {
        status::OK => {
            let bytes_transferred = raw_msg.bytes_transferred;

            Message::<T>::try_from(raw_msg)
                .map(|msg| (msg, bytes_transferred))
        },
        r => Err(r),
    }
}

pub mod kernel {
    pub const EXCEPTION: i32 = 0xF0;
    pub const IRQ: i32 = 0xF1;
    pub const EXIT: i32 = 0xF2;
    use super::RawMessage;
    use crate::Tid;
    use crate::address::VAddr;
    use crate::syscall::c_types::CTid;
    use core::ffi::c_void;
    use core::convert::TryFrom;
    use core::{mem, result};
    use crate::error;

    #[derive(Clone)]
    #[repr(C)]
    pub struct RawExceptionMessage {
        pub who: CTid,
        pub error_code: i32,
        pub fault_address: *const c_void,
        pub int_num: i32,
    }

    impl RawExceptionMessage {
        pub const PRESENT: u32 = 0x01;
        pub const WRITE: u32 = 0x02;
        pub const USER: u32 = 0x04;
        pub const RESD_WRITE: u32 = 0x08;
        pub const FETCH: u32 = 0x10;
    }

    pub struct ExceptionMessage {
        pub int_num: u32,
        pub who: Tid,
        pub error_code: u32,
        pub fault_address: VAddr,
    }

    impl ExceptionMessage {
        pub const PRESENT: u32 = RawExceptionMessage::PRESENT;
        pub const WRITE: u32 = RawExceptionMessage::WRITE;
        pub const USER: u32 = RawExceptionMessage::USER;
        pub const RESD_WRITE: u32 = RawExceptionMessage::RESD_WRITE;
        pub const FETCH: u32 = RawExceptionMessage::FETCH;

        pub fn new(raw_msg: RawExceptionMessage) -> Self {
            ExceptionMessage {
                int_num: raw_msg.int_num as u32,
                who: Tid::new(raw_msg.who),
                error_code: raw_msg.error_code as u32,
                fault_address: raw_msg.fault_address as VAddr,
            }
        }
    }

    impl From<RawExceptionMessage> for ExceptionMessage {
        fn from(value: RawExceptionMessage) -> Self {
            Self::new(value)
        }
    }

    impl TryFrom<RawMessage> for ExceptionMessage {
        type Error = i32;
        fn try_from(value: RawMessage) -> result::Result<Self, Self::Error> {
            RawExceptionMessage::try_from(value)
                .map(|raw_msg| Self::from(raw_msg))
        }
    }

    impl TryFrom<RawMessage> for RawExceptionMessage {
        type Error = i32;

        fn try_from(msg: RawMessage) -> Result<Self, Self::Error> {
            if msg.buffer_len < mem::size_of::<Self>() {
                Err(error::PARSE_ERROR)
            } else {
                let int_num_arr;
                let who_arr;
                let code_arr;
                let fault_addr_arr;

                let int_num_ptr = (msg.buffer.wrapping_add(offset_of!(RawExceptionMessage, int_num))) as *const [u8; mem::size_of::<i32>()];
                let who_ptr = (msg.buffer.wrapping_add(offset_of!(RawExceptionMessage, who))) as *const [u8; mem::size_of::<CTid>()];
                let code_ptr = (msg.buffer.wrapping_add(offset_of!(RawExceptionMessage, error_code))) as *const [u8; mem::size_of::<i32>()];
                let fault_addr_ptr = (msg.buffer.wrapping_add(offset_of!(RawExceptionMessage, fault_address))) as *const [u8; mem::size_of::<usize>()];

                unsafe {
                    int_num_arr = int_num_ptr.read();
                    who_arr = who_ptr.read();
                    code_arr = code_ptr.read();
                    fault_addr_arr = fault_addr_ptr.read();
                }

                Ok(Self {
                    who: CTid::from_le_bytes(who_arr),
                    error_code: i32::from_le_bytes(code_arr),
                    fault_address: usize::from_le_bytes(fault_addr_arr) as *const c_void,
                    int_num: i32::from_le_bytes(int_num_arr),
                })
            }
        }
    }

    #[derive(Clone)]
    #[repr(C)]
    pub struct RawExitMessage {
        pub who: CTid,
        pub status_code: i32,
    }

    pub struct ExitMessage {
        pub who: Tid,
        pub status_code: i32,
    }

    impl ExitMessage {
        pub fn new(raw_msg: RawExitMessage) -> Self {
            ExitMessage {
                who: raw_msg.who.into(),
                status_code: raw_msg.status_code
            }
        }
    }

    impl From<RawExitMessage> for ExitMessage {
        fn from(value: RawExitMessage) -> Self {
            Self::new(value)
        }
    }

    impl TryFrom<RawMessage> for ExitMessage {
        type Error = i32;
        fn try_from(value: RawMessage) -> result::Result<Self, Self::Error> {
            RawExitMessage::try_from(value)
                .map(|raw_msg| Self::from(raw_msg))
        }
    }

    impl TryFrom<RawMessage> for RawExitMessage {
        type Error = i32;

        fn try_from(msg: RawMessage) -> Result<Self, Self::Error> {
            if msg.buffer_len < mem::size_of::<RawExitMessage>() {
                Err(error::PARSE_ERROR)
            } else {
                let who_arr;
                let status_arr;

                let who_ptr = (msg.buffer.wrapping_add(offset_of!(RawExitMessage, who))) as *const [u8; mem::size_of::<CTid>()];
                let status_ptr = (msg.buffer.wrapping_add(offset_of!(RawExitMessage, status_code))) as *const [u8; mem::size_of::<i32>()];

                unsafe {
                    who_arr = who_ptr.read();
                    status_arr = status_ptr.read();
                }

                Ok(RawExitMessage {
                        who: CTid::from_le_bytes(who_arr),
                        status_code: i32::from_le_bytes(status_arr),
                    }
                )
            }
        }
    }
}

pub mod init {
    use super::RawMessage;
    use crate::Tid;
    use crate::address::VAddr;
    use crate::page::VirtualPage;
    use crate::error::{BAD_REQUEST, INVALID_NAME, ZERO_LENGTH, INVALID_ADDRESS};
    use crate::error;
    use alloc::string::String;
    use alloc::vec::Vec;
    use crate::device::DeviceId;
    use core::ffi::c_void;
    use crate::message::{Message, BlankMessage};
    use crate::syscall::c_types::{CTid};
    use super::Result;
    use core::ptr;
    use core::convert::TryFrom;
    use core::result;
    use alloc::boxed::Box;
    use core::mem;
    
    pub const MAX_NAME_LEN: usize = 32;

    pub const MAP: i32 = 1;
    pub const UNMAP: i32 = 2;
    pub const REGISTER_SERVER: i32 = 3;
    pub const UNREGISTER_SERVER: i32 = 4;

    pub const CREATE_PORT: i32 = 5;
    pub const DESTROY_PORT: i32 = 6;
    pub const SEND_PORT: i32 = 7;
    pub const RECEIVE_PORT: i32 = 8;

    pub const REGISTER_NAME: i32 = 9;
    pub const LOOKUP_NAME: i32 = 10;
    pub const UNREGISTER_NAME: i32 = 11;

    pub const MAP_IO: i32 = 12;         // Reserve an IO port range
    pub const UNMAP_IO: i32 = 13;       // Release an IO port range

    pub trait Valid {
        fn validate(&self) -> Result<()>;
    }

    pub trait Name {
        fn name_vec(&self) -> &Vec<u8>;

        fn is_valid_name(&self) -> bool {
            self.name_string().is_ok()
        }

        fn validate(&self) -> Result<()> {
            self.name_string().map(|_| ())
        }

        fn name_string(&self) -> Result<String> {
            String::from_utf8(self.name_vec().clone()).map_err(|_| INVALID_NAME)
        }
    }

    pub trait NameString {
        fn name_vec(&self) -> &Vec<u8>;

        fn is_valid_name(&self) -> bool {
            self.name_string().is_ok()
        }

        fn name_string(&self) -> Result<String> {
            String::from_utf8(self.name_vec().clone()).map_err(|_| INVALID_NAME)
        }
        fn validate(&self) -> Result<()> {
            self.name_string()
                .and_then(|s| {
                    if s.as_bytes().len() > MAX_NAME_LEN {
                        Err(INVALID_NAME)
                    } else {
                        Ok(())
                    }
                })
        }
    }

    pub trait SimpleResponse {
        fn new_message(recipient: Tid, is_success: bool) -> BlankMessage {
            Message {
                subject: if is_success { RawMessage::RESPONSE_OK } else { RawMessage::RESPONSE_FAIL },
                sender: Tid::null(),
                recipient: recipient.into(),
                data: None,
                bytes_transferred: None,
                flags: 0
            }
        }
    }

    #[repr(C)]
    #[derive(Clone)]
    pub struct RawMapRequest {
        pub address: *const c_void,
        pub device: i32,
        pub length: usize,
        pub offset: usize,
        pub flags: i32,
    }

    impl TryFrom<RawMessage> for RawMapRequest {
        type Error = i32;

        fn try_from(msg: RawMessage) -> result::Result<Self, Self::Error> {
            if msg.buffer_len < mem::size_of::<RawMapRequest>() {
                Err(error::PARSE_ERROR)
            } else {
                let address_arr;
                let device_arr;
                let length_arr;
                let offset_arr;
                let flags_arr;
                
                let address_ptr = (msg.buffer.wrapping_add(offset_of!(RawMapRequest, address))) as *const [u8; mem::size_of::<usize>()];
                let device_ptr = (msg.buffer.wrapping_add(offset_of!(RawMapRequest, device))) as *const [u8; mem::size_of::<i32>()];
                let length_ptr = (msg.buffer.wrapping_add(offset_of!(RawMapRequest, length))) as *const [u8; mem::size_of::<usize>()];
                let offset_ptr = (msg.buffer.wrapping_add(offset_of!(RawMapRequest, offset))) as *const [u8; mem::size_of::<usize>()];
                let flags_ptr = (msg.buffer.wrapping_add(offset_of!(RawMapRequest, flags))) as *const [u8; mem::size_of::<i32>()];

                unsafe {
                    address_arr = address_ptr.read();
                    device_arr = device_ptr.read();
                    length_arr = length_ptr.read();
                    offset_arr = offset_ptr.read();
                    flags_arr = flags_ptr.read();
                }

                Ok(RawMapRequest {
                    address: usize::from_le_bytes(address_arr) as *const c_void,
                    device: i32::from_le_bytes(device_arr),
                    length: usize::from_le_bytes(length_arr),
                    offset: usize::from_le_bytes(offset_arr),
                    flags: i32::from_le_bytes(flags_arr)
                })
            }
        }
    }
    
    pub struct MapRequest {
        pub address: Option<VAddr>,
        pub device: DeviceId,
        pub length: usize,
        pub offset: usize,
        pub flags: u32,
    }

    impl From<RawMapRequest> for MapRequest {
        fn from(raw_msg: RawMapRequest) -> Self {
            MapRequest {
                address: if raw_msg.address == ptr::null() {
                    None
                } else {
                    Some(raw_msg.address as VAddr)
                },
                device: DeviceId::new(raw_msg.device as u32),
                length: raw_msg.length,
                offset: raw_msg.offset,
                flags: raw_msg.flags as u32,
            }
        }
    }

    impl TryFrom<RawMessage> for MapRequest {
        type Error = i32;
        fn try_from(value: RawMessage) -> result::Result<Self, Self::Error> {
            RawMapRequest::try_from(value)
                .map(|request| Self::from(request))
        }
    }

    impl MapRequest {
        pub fn new(msg: Message<RawMapRequest>) -> Result<Self> {
            msg.data()
                .map(|raw_msg| MapRequest::from(raw_msg.clone()))
                .ok_or(BAD_REQUEST)
                .and_then(|request| request.validate().map(move |_| request))
        }
    }

    impl Valid for MapRequest {
        fn validate(&self) -> Result<()> {
            if self.length == 0 {
                Err(ZERO_LENGTH)
            } else if let Some(address) = self.address {
                if address.align_offset(VirtualPage::SMALL_PAGE_SIZE) != 0 {
                    Err(INVALID_ADDRESS)
                } else {
                    Ok(())
                }
            } else {
                Ok(())
            }
        }
    }
    
    pub struct MapResponse {}

    impl MapResponse {
        pub fn new_message(recipient: Tid, new_address: Option<VAddr>) -> Message<VAddr> {
            match new_address {
                None => Message {
                    subject: RawMessage::RESPONSE_FAIL,
                    sender: Tid::null(),
                    recipient: recipient.clone(),
                    data: None,
                    bytes_transferred: None,
                    flags: 0,
                },
                Some(address) => Message {
                    subject: RawMessage::RESPONSE_OK,
                    sender: Tid::null(),
                    recipient: recipient.into(),
                    data: Some(Box::new(address.clone())),
                    bytes_transferred: None,
                    flags: 0
                }
            }
        }
    }

    #[derive(Clone)]
    #[repr(C)]
    pub struct RawUnmapRequest {
        pub address: *const c_void,
        pub length: usize,
    }

    impl TryFrom<RawMessage> for RawUnmapRequest {
        type Error = i32;

        fn try_from(msg: RawMessage) -> result::Result<Self, Self::Error> {
            if msg.buffer_len < mem::size_of::<RawUnmapRequest>() {
                Err(error::PARSE_ERROR)
            } else {
                let address_arr;
                let length_arr;

                let address_ptr = (msg.buffer.wrapping_add(offset_of!(RawUnmapRequest, address))) as *const [u8; mem::size_of::<usize>()];
                let length_ptr = (msg.buffer.wrapping_add(offset_of!(RawUnmapRequest, length))) as *const [u8; mem::size_of::<usize>()];

                unsafe {
                    address_arr = address_ptr.read();
                    length_arr = length_ptr.read();
                }

                Ok(RawUnmapRequest {
                    address: usize::from_le_bytes(address_arr) as *const c_void,
                    length: usize::from_le_bytes(length_arr),
                })
            }
        }
    }

    pub struct UnmapRequest {
        pub address: VAddr,
        pub length: usize,
    }

    impl TryFrom<RawUnmapRequest> for UnmapRequest {
        type Error = i32;

        fn try_from(raw_msg: RawUnmapRequest) -> result::Result<Self, Self::Error> {
            if raw_msg.address.is_null() {
                result::Result::Err(error::PARSE_ERROR)
            } else {
                result::Result::Ok(UnmapRequest {
                    address: raw_msg.address as VAddr,
                    length: raw_msg.length,
                })
            }
        }
    }

    impl TryFrom<RawMessage> for UnmapRequest {
        type Error = i32;
        fn try_from(value: RawMessage) -> result::Result<Self, Self::Error> {
            RawUnmapRequest::try_from(value)
                .and_then(|request| Self::try_from(request))
        }
    }

    impl UnmapRequest {
        pub fn new(msg: Message<RawUnmapRequest>) -> Result<Self> {
            msg.data()
                .ok_or(BAD_REQUEST)
                .and_then(|raw_msg| UnmapRequest::try_from(raw_msg.clone()))
                .and_then(|request| request.validate().map(move |_| request))
        }
    }

    impl Valid for UnmapRequest {
        fn validate(&self) -> Result<()> {
            if self.length == 0 {
                Err(ZERO_LENGTH)
            } else if self.address.align_offset(VirtualPage::SMALL_PAGE_SIZE) != 0 {
                Err(INVALID_ADDRESS)
            } else {
                Ok(())
            }
        }
    }
    
    pub struct UnmapResponse {}
    impl SimpleResponse for UnmapResponse {}

    /*
    pub struct CreatePortRequest {
        pub pid: Pid,
        pub flags: u32,
    }

    impl CreatePortRequest {
        pub fn new(message: RawMessage) -> Result<Self> {
            let request;

            unsafe {
                request = Self {
                    pid: message.payload.u16[0].into(),
                    flags: message.payload.u32[1],
                }
            }

            match request.validate() {
                Err(x) => Err(x),
                Ok(_) => Ok(request),
            }
        }
    }

    impl Valid for CreatePortRequest {
        fn validate(&self) -> Result<()> {
            if self.pid.is_null() {
                Err(INVALID_PORT)
            } else {
                Ok(())
            }
        }
    }
    
    pub struct CreatePortResponse {}

    impl CreatePortResponse {
        pub fn new_message(recipient: Tid, new_pid: Option<Pid>) -> RawMessage {
            let mut payload = ShortMessagePayload::new();

            let subject = match new_pid {
                Some(pid) => {
                    unsafe {
                        payload.u16[0] = pid.into();
                    }
                    RawMessage::RESPONSE_OK
                },
                None => RawMessage::RESPONSE_FAIL
            };

            RawMessage::new(subject, recipient.into(), payload)
        }
    }

    
    pub struct DestroyPortRequest {
        pub pid: Pid,
    }

    impl DestroyPortRequest {
        pub fn new(message: RawMessage) -> Result<Self> {
            let request;

            unsafe {
                request = Self {
                    pid: message.payload.u16[0].into(),
                }
            }

            match request.validate() {
                Err(x) => Err(x),
                Ok(_) => Ok(request),
            }
        }
    }

    impl Valid for DestroyPortRequest {
        fn validate(&self) -> Result<()> {
            if self.pid.is_null() {
                Err(INVALID_PORT)
            } else {
                Ok(())
            }
        }
    }

    pub struct DestroyPortResponse {}

    impl DestroyPortResponse {
        pub fn new_message(recipient: Tid, is_ok: bool) -> RawMessage {
            let subject = if is_ok {
                RawMessage::RESPONSE_OK
            } else {
                RawMessage::RESPONSE_FAIL
            };

            RawMessage::new_short(subject, recipient.into())
        }
    }

    
    pub struct SendPortRequest {
        pub pid: Pid,
        pub buffer: VAddr,
        pub buffer_len: usize,
    }

    impl SendPortRequest {
        pub fn new(message: super::RawMessage) -> Result<Self> {
            let request;

            unsafe {
                request = Self {
                    pid: message.payload.u16[0].into(),
                    buffer: (message.payload.u16[1] as usize | ((message.payload.u16[2] as usize) << 16)) as VAddr,
                    buffer_len: (message.payload.u16[3] as usize) | ((message.payload.u16[4] as usize) << 16),
                }
            }

            match request.validate() {
                Err(x) => Err(x),
                Ok(_) => Ok(request),
            }
        }
    }

    impl Valid for SendPortRequest {
        fn validate(&self) -> Result<()> {
            if self.buffer_len == 0 {
                Err(ZERO_LENGTH)
            } else if self.buffer == NULL_VADDR {
                Err(INVALID_ADDRESS)
            } else if self.pid.is_null() {
                Err(INVALID_PORT)
            }
            else {
                Ok(())
            }
        }
    }
    
    pub struct SendPortResponse {}

    impl SendPortResponse {
        pub fn new_message(recipient: Tid, sent_bytes: Option<usize>) -> RawMessage {
            let mut payload = ShortMessagePayload::new();

            let subject = if let Some(b) = sent_bytes {
                unsafe {
                    payload.u32[0] = b as u32;
                }
                RawMessage::RESPONSE_OK
            } else {
                RawMessage::RESPONSE_FAIL
            };

            RawMessage::new(subject, recipient.into(), payload)
        }
    }

    
    pub struct ReceivePortRequest {
        pub pid: Pid,
        pub buffer: VAddr,
        pub buffer_len: usize,
    }

    impl ReceivePortRequest {
        pub fn new(message: super::RawMessage) -> Result<Self> {
            let request;

            unsafe {
                request = Self {
                    pid: message.payload.u16[0].into(),
                    buffer: (message.payload.u16[1] as usize | ((message.payload.u16[2] as usize) << 16)) as VAddr,
                    buffer_len: (message.payload.u16[3] as usize) | ((message.payload.u16[4] as usize) << 16),
                }
            }

            request.validate().map(|_| request)
        }
    }

    impl Valid for ReceivePortRequest {
        fn validate(&self) -> Result<()> {
            if self.buffer_len == 0 {
                Err(ZERO_LENGTH)
            } else if self.buffer == NULL_VADDR {
                Err(INVALID_ADDRESS)
            } else {
                Ok(())
            }
        }
    }

    pub struct ReceivePortResponse {}

    impl ReceivePortResponse {
        pub fn new_message(recipient: Tid, received_info: Option<(Tid, usize)>) -> RawMessage {
            let mut payload = ShortMessagePayload::new();

            let subject = if let Some((tid, bytes)) = received_info {
                unsafe {
                    payload.u32[0] = bytes as u32;
                    payload.u16[2] = tid.into();
                }
                RawMessage::RESPONSE_OK
            } else {
                RawMessage::RESPONSE_FAIL
            };

            RawMessage::new(subject, recipient.into(), payload)
        }
    }
*/
    #[derive(Clone)]
    #[repr(C)]
    pub struct RawRegisterServerRequest {
        pub server_type: i32,
    }

    impl TryFrom<RawMessage> for RawRegisterServerRequest {
        type Error = i32;

        fn try_from(msg: RawMessage) -> result::Result<Self, Self::Error> {
            if msg.buffer_len < mem::size_of::<RawRegisterServerRequest>() {
                Err(error::PARSE_ERROR)
            } else {
                let server_arr;
                let server_ptr = msg.buffer as *const [u8; mem::size_of::<i32>()];

                unsafe {
                    server_arr = server_ptr.read();
                }

                Ok(RawRegisterServerRequest {
                    server_type: i32::from_le_bytes(server_arr),
                })
            }
        }
    }

    pub struct RegisterServerRequest {
        pub server_type: i32,
    }

    impl RegisterServerRequest {
        pub fn new(msg: Message<RawRegisterServerRequest>) -> Result<Self> {
            msg.data()
                .ok_or(BAD_REQUEST)
                .map(|raw_msg| RegisterServerRequest::from(raw_msg.clone()))
                .and_then(|request| request.validate().map(move |_| request))
        }
    }

    impl From<RawRegisterServerRequest> for RegisterServerRequest {
        fn from(raw_msg: RawRegisterServerRequest) -> Self {
            Self {
                server_type: raw_msg.server_type,
            }
        }
    }

    impl TryFrom<RawMessage> for RegisterServerRequest {
        type Error = i32;
        fn try_from(value: RawMessage) -> result::Result<Self, Self::Error> {
            RawRegisterServerRequest::try_from(value)
                .map(|request| Self::from(request))
        }
    }

    impl Valid for RegisterServerRequest {
        fn validate(&self) -> Result<()> {
            Ok(())
        }
    }

    pub struct RegisterServerResponse {}
    impl SimpleResponse for RegisterServerResponse {}

    #[repr(C)]
    #[derive(Clone)]
    pub struct RawRegisterNameRequest {
        pub name: [u8; MAX_NAME_LEN],
    }

    impl TryFrom<RawMessage> for RawRegisterNameRequest {
        type Error = i32;

        fn try_from(msg: RawMessage) -> result::Result<Self, Self::Error> {
            if msg.buffer_len < mem::size_of::<RawRegisterNameRequest>() {
                Err(error::PARSE_ERROR)
            } else {
                let name_ptr = msg.buffer as *const [u8; MAX_NAME_LEN];
                let name_arr = unsafe { name_ptr.read() };

                Ok(RawRegisterNameRequest {
                    name: name_arr,
                })
            }
        }
    }

    pub struct RegisterNameRequest {
        pub name: Vec<u8>,
    }

    impl From<RawRegisterNameRequest> for RegisterNameRequest {
        fn from(raw_msg: RawRegisterNameRequest) -> Self {
            let mut iter = raw_msg.name.split(|c| *c == 0);

            let s = match iter.next() {
                Some(slice) => slice,
                None => &raw_msg.name,
            };

            Self {
                name: Vec::from(s)
            }
        }
    }

    impl TryFrom<RawMessage> for RegisterNameRequest {
        type Error = i32;
        fn try_from(value: RawMessage) -> result::Result<Self, Self::Error> {
            RawRegisterNameRequest::try_from(value)
                .map(|request| Self::from(request))
        }
    }

    impl RegisterNameRequest {
        pub fn new(msg: Message<RawRegisterNameRequest>) -> Result<Self> {
            msg.data()
                .ok_or(BAD_REQUEST)
                .map(|raw_msg| RegisterNameRequest::from(raw_msg.clone()))
                .and_then(|request| request.validate().map(move |_| request))
        }
    }

    /*
    impl Valid for RegisterNameRequest {

    }

    impl Name for RegisterNameRequest {

    }
*/
    impl NameString for RegisterNameRequest {
        fn name_vec(&self) -> &Vec<u8> {
            &self.name
        }
    }

    pub struct RegisterNameResponse {}
    impl SimpleResponse for RegisterNameResponse {}

    #[repr(C)]
    #[derive(Clone)]
    pub struct RawUnregisterNameRequest {
        pub name: [u8; MAX_NAME_LEN],
    }

    impl TryFrom<RawMessage> for RawUnregisterNameRequest {
        type Error = i32;

        fn try_from(msg: RawMessage) -> result::Result<Self, Self::Error> {
            if msg.buffer_len < mem::size_of::<RawUnregisterNameRequest>() {
                Err(error::PARSE_ERROR)
            } else {
                let name_ptr = msg.buffer as *const [u8; MAX_NAME_LEN];
                let name_arr = unsafe { name_ptr.read() };

                Ok(RawUnregisterNameRequest {
                    name: name_arr,
                })
            }
        }
    }

    impl TryFrom<RawMessage> for UnregisterNameRequest {
        type Error = i32;
        fn try_from(value: RawMessage) -> result::Result<Self, Self::Error> {
            RawUnregisterNameRequest::try_from(value)
                .map(|request| Self::from(request))
        }
    }

    pub struct UnregisterNameRequest {
        pub name: Vec<u8>,
    }

    impl From<RawUnregisterNameRequest> for UnregisterNameRequest {
        fn from(raw_msg: RawUnregisterNameRequest) -> Self {
            let mut iter = raw_msg.name.split(|c| *c == 0);

            let s = match iter.next() {
                Some(slice) => slice,
                None => &raw_msg.name,
            };

            Self {
                name: Vec::from(s)
            }
        }
    }

    impl NameString for UnregisterNameRequest {
        fn name_vec(&self) -> &Vec<u8> {
            &self.name
        }
    }

    pub struct UnregisterNameResponse {}
    impl SimpleResponse for UnregisterNameResponse {}


    #[repr(C)]
    #[derive(Clone)]
    pub struct RawLookupNameRequest {
        pub name: [u8; MAX_NAME_LEN],
    }

    impl TryFrom<RawMessage> for RawLookupNameRequest {
        type Error = i32;

        fn try_from(msg: RawMessage) -> result::Result<Self, Self::Error> {
            if msg.buffer_len < mem::size_of::<RawLookupNameRequest>() {
                Err(error::PARSE_ERROR)
            } else {
                let name_ptr = msg.buffer as *const [u8; MAX_NAME_LEN];
                let name_arr = unsafe { name_ptr.read() };

                Ok(RawLookupNameRequest {
                    name: name_arr,
                })
            }
        }
    }

    impl TryFrom<RawMessage> for LookupNameRequest {
        type Error = i32;
        fn try_from(value: RawMessage) -> result::Result<Self, Self::Error> {
            RawLookupNameRequest::try_from(value)
                .map(|request| Self::from(request))
        }
    }

    pub struct LookupNameRequest {
        pub name: Vec<u8>
    }

    impl From<RawLookupNameRequest> for LookupNameRequest {
        fn from(raw_msg: RawLookupNameRequest) -> Self {
            let mut iter = raw_msg.name.split(|c| *c == 0);

            let s = match iter.next() {
                Some(slice) => slice,
                None => &raw_msg.name,
            };

            Self {
                name: Vec::from(s)
            }
        }
    }

    impl NameString for LookupNameRequest {
        fn name_vec(&self) -> &Vec<u8> {
            &self.name
        }
    }

    pub struct LookupNameResponse {}

    impl LookupNameResponse {
        pub fn new_message(recipient: Tid, tid_option: Option<Tid>) -> Message<CTid> {
            match tid_option {
                None => Message {
                    subject: RawMessage::RESPONSE_FAIL,
                    sender: Tid::null(),
                    recipient: recipient.clone(),
                    data: None,
                    bytes_transferred: None,
                    flags: 0,
                },
                Some(tid) => Message {
                    subject: RawMessage::RESPONSE_OK,
                    sender: Tid::null(),
                    recipient: recipient.into(),
                    data: Some(Box::new(tid.into())),
                    bytes_transferred: None,
                    flags: 0
                }
            }
        }
    }

    #[repr(C)]
    #[derive(Clone)]
    pub struct RawMapIoRequest {
        start: u16,
        length: u16,
    }

    pub struct MapIoRequest {
        start: u16,
        length: u16,
    }

    impl MapIoRequest {
        fn new(raw_msg: RawMapIoRequest) -> Self {
            Self {
                start: raw_msg.start,
                length: raw_msg.length,
            }
        }
    }

    impl From<RawMapIoRequest> for MapIoRequest {
        fn from(raw_msg: RawMapIoRequest) -> Self {
            Self::new(raw_msg)
        }
    }

    impl TryFrom<RawMessage> for RawMapIoRequest {
        type Error = i32;

        fn try_from(msg: RawMessage) -> result::Result<Self, Self::Error> {
            if msg.buffer_len < mem::size_of::<RawMapIoRequest>() {
                Err(error::PARSE_ERROR)
            } else {
                let start_ptr = (msg.buffer.wrapping_add(offset_of!(RawMapIoRequest, start))) as *const [u8; mem::size_of::<u16>()];
                let start_arr = unsafe { start_ptr.read() };

                let length_ptr = (msg.buffer.wrapping_add(offset_of!(RawMapIoRequest, length))) as *const [u8; mem::size_of::<u16>()];
                let length_arr = unsafe { length_ptr.read() };

                Ok(RawMapIoRequest {
                    start: u16::from_le_bytes(start_arr),
                    length: u16::from_le_bytes(length_arr),
                })
            }
        }
    }

    impl TryFrom<RawMessage> for MapIoRequest {
        type Error = i32;
        fn try_from(value: RawMessage) -> result::Result<Self, Self::Error> {
            RawMapIoRequest::try_from(value)
                .map(|request| Self::from(request))
        }
    }

    #[repr(C)]
    #[derive(Clone)]
    pub struct RawUnmapIoRequest {
        start: u16,
        length: u16,
    }

    pub struct UnmapIoRequest {
        start: u16,
        length: u16,
    }

    impl UnmapIoRequest {
        fn new(raw_msg: RawUnmapIoRequest) -> Self {
            Self {
                start: raw_msg.start,
                length: raw_msg.length,
            }
        }
    }

    impl From<RawUnmapIoRequest> for UnmapIoRequest {
        fn from(raw_msg: RawUnmapIoRequest) -> Self {
            Self::new(raw_msg)
        }
    }

    impl TryFrom<RawMessage> for RawUnmapIoRequest {
        type Error = i32;

        fn try_from(msg: RawMessage) -> result::Result<Self, Self::Error> {
            if msg.buffer_len < mem::size_of::<RawUnmapIoRequest>() {
                Err(error::PARSE_ERROR)
            } else {
                let start_ptr = (msg.buffer.wrapping_add(offset_of!(RawUnmapIoRequest, start))) as *const [u8; mem::size_of::<u16>()];
                let start_arr = unsafe { start_ptr.read() };

                let length_ptr = (msg.buffer.wrapping_add(offset_of!(RawUnmapIoRequest, length))) as *const [u8; mem::size_of::<u16>()];
                let length_arr = unsafe { length_ptr.read() };

                Ok(RawUnmapIoRequest {
                    start: u16::from_le_bytes(start_arr),
                    length: u16::from_le_bytes(length_arr),
                })
            }
        }
    }

    impl TryFrom<RawMessage> for UnmapIoRequest {
        type Error = i32;
        fn try_from(value: RawMessage) -> result::Result<Self, Self::Error> {
            RawUnmapIoRequest::try_from(value)
                .map(|request| Self::from(request))
        }
    }
}
