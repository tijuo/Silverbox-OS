use core::result;
pub use crate::types::Tid;

pub enum MessageError {
    UnknownSubject,
    MalformedPayload
}

pub type Result<T> = result::Result<T, MessageError>;

/// Indicates that a type can be sent/received via kernel message passing
pub trait Transmit {
    type Payload;

    /// Retrieves the message payload from the received message
    fn read_payload(header: &MessageHeader) -> Result<Self::Payload>;

    /// Sets the message payload to be sent
    fn write_payload(&self);
}

pub enum MessageTarget {
    Sender(Tid),
    Recipient(Option<Tid>),
}

pub enum PayloadSize {
    Standard,
    Empty
}

pub struct MessageHeader {
    pub target: MessageTarget,
    pub flags: u32,
    pub subject: u32,
}

impl MessageHeader {
    pub const MSG_NOBLOCK: u32 = 1;
    pub const MSG_STD: u32 = 0;
    pub const MSG_EMPTY: u32 = 2;
    pub const MSG_KERNEL: u32 = 0x80000000;

    pub fn new_to_recipient(recipient: &Option<Tid>, subject: u32, flags: u32) -> MessageHeader {
        MessageHeader {
            target: MessageTarget::Recipient(recipient.clone()),
            subject,
            flags
        }
    }

    pub fn new_from_sender(sender: &Tid, subject: u32, flags: u32) -> MessageHeader {
        MessageHeader {
            target: MessageTarget::Sender(sender.clone()),
            subject,
            flags
        }
    }

    pub fn payload_size(&self) -> PayloadSize {
        if self.flags & Self::MSG_EMPTY == Self::MSG_EMPTY {
            PayloadSize::Empty
        } else {
            PayloadSize::Standard
        }
    }

    pub fn recipient(&self) -> Option<&Tid> {
        if let MessageTarget::Recipient(Some(ref tid))=self.target {
            Some(tid)
        } else {
            None
        }
    }

    pub fn sender(&self) -> Option<&Tid> {
        if let MessageTarget::Sender(ref tid)=self.target {
            Some(tid)
        } else {
            None
        }
    }
}

pub mod kernel {
    use crate::message::MessageError;
    use super::{Transmit, MessageHeader, Result, PayloadSize};
    use crate::types::CTid;

    pub const EXCEPTION: u32 = 0xFFFFFFFF;
    pub const EXIT: u32 = 0xFFFFFFFE;
    pub const LOW_MEMORY: u32 = 0xFFFFFFFD;
    pub const OUT_OF_MEMORY: u32 = 0xFFFFFFFC;

    #[derive(Clone, Default)]
    #[repr(C, align(16))]
    pub struct ExceptionMessagePayload {
        pub eax: u32,
        pub ebx: u32,
        pub ecx: u32,
        pub edx: u32,
        pub esi: u32,
        pub edi: u32,
        pub ebp: u32,
        pub esp: u32,
        pub cs: u16,
        pub ds: u16,
        pub es: u16,
        pub fs: u16,
        pub gs: u16,
        pub ss: u16,
        pub eflags: u32,
        pub cr0: u32,
        pub cr2: u32,
        pub cr3: u32,
        pub cr4: u32,
        pub eip: u32,
        pub error_code: u32,
        pub fault_num: u8,
        pub processor_id: u8,
        pub who: CTid,
    }

    impl ExceptionMessagePayload {
        // Error codes

        pub const PRESENT: u32 = 0x01;
        pub const WRITE: u32 = 0x02;
        pub const USER: u32 = 0x04;
        pub const RESD_WRITE: u32 = 0x08;
        pub const FETCH: u32 = 0x10;
    }

    impl Transmit for ExceptionMessagePayload {
        type Payload = Self;
        fn read_payload(header: &MessageHeader) -> Result<Self::Payload> {
            match header.subject {
                EXCEPTION => {
                    if let PayloadSize::Empty = header.payload_size() {
                        Err(MessageError::MalformedPayload)
                    } else {
                        let mut payload = ExceptionMessagePayload::default();

                        unsafe {
                            asm!(
                            "movapd [{0}], xmm0",
                            "movapd [{0}+16], xmm1",
                            "movapd [{0}+32], xmm2",
                            "movapd [{0}+48], xmm3",
                            "movapd [{0}+64], xmm4",
                            in (reg) &mut payload,
                            lateout("xmm0") _,
                            lateout("xmm1") _,
                            lateout("xmm2") _,
                            lateout("xmm3") _,
                            lateout("xmm4") _,
                            );
                        }

                        Ok(payload)
                    }
                },
                _ => Err(MessageError::UnknownSubject)
            }
        }

        fn write_payload(&self) {
            unsafe {
                asm!(
                "movapd xmm0, [{0}]",
                "movapd xmm1, [{0}+16]",
                "movapd xmm2, [{0}+32]",
                "movapd xmm3, [{0}+48]",
                "movapd xmm4, [{0}+64]",
                "xorpd xmm5, xmm5",
                "xorpd xmm6, xmm6",
                "xorpd xmm7, xmm7",
                in (reg) &self,
                lateout("xmm0") _,
                lateout("xmm1") _,
                lateout("xmm2") _,
                lateout("xmm3") _,
                lateout("xmm4") _,
                lateout("xmm5") _,
                lateout("xmm6") _,
                lateout("xmm7") _,
                );
            }
        }
    }

    #[derive(Clone, Default)]
    #[repr(C, align(16))]
    pub struct ExitMessagePayload {
        pub exit_code: i32,
        pub who: CTid,
    }

    impl Transmit for ExitMessagePayload {
        type Payload = Self;
        fn read_payload(header: &MessageHeader) -> Result<Self::Payload> {
            match header.subject {
                EXIT => {
                    if let PayloadSize::Empty = header.payload_size() {
                        Err(MessageError::MalformedPayload)
                    } else {
                        let mut payload = ExitMessagePayload::default();

                        unsafe {
                            asm!(
                            "movd [{0}], xmm0",
                            "movd [{0}+4], xmm1",
                            in (reg) &mut payload,
                            );
                        }

                        Ok(payload)
                    }
                },
                _ => Err(MessageError::UnknownSubject),
            }
        }

        fn write_payload(&self) {
            unsafe {
                asm!(
                "movd xmm0, [{0}]",
                "movd xmm1, [{0}+16]",
                "xorpd xmm2, xmm2",
                "xorpd xmm3, xmm3",
                "xorpd xmm4, xmm4",
                "xorpd xmm5, xmm5",
                "xorpd xmm6, xmm6",
                "xorpd xmm7, xmm7",
                in (reg) &self,
                lateout("xmm0") _,
                lateout("xmm1") _,
                lateout("xmm2") _,
                lateout("xmm3") _,
                lateout("xmm4") _,
                lateout("xmm5") _,
                lateout("xmm6") _,
                lateout("xmm7") _,
                );
            }
        }
    }
}

#[repr(C)]
pub union RawPayload {
    pub uint64: [u64; 16],
    pub int64: [i64; 16],
    pub uint32: [u32; 32],
    pub int32: [i32; 32],
    pub uint16: [u16; 64],
    pub int16: [i16; 64],
    pub uint8: [u8; 128],
    pub int8: [i8; 128],
}
