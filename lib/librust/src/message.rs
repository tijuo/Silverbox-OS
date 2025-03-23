pub use crate::types::{Tid, CTid};

#[derive(Clone)]
pub struct MessageHeader {
    pub target: Option<Tid>,
    pub flags: u16,
    pub subject: u32,
}

impl MessageHeader {
    pub const MSG_NOBLOCK: u16 = 1;
    pub const MSG_STD: u16 = 0;
    pub const MSG_EMPTY: u16 = 2;
    pub const MSG_KERNEL: u16 = 0x8000;
    pub const ANY: Option<Tid> = None;
    pub const ANY_SENDER: Option<Tid> = MessageHeader::ANY;
    pub const ANY_RECIPIENT: Option<Tid> = MessageHeader::ANY;

    pub const fn new(target: Option<Tid>, subject: u32, flags: u16) -> Self {
        Self { target, flags, subject }
    }

    pub const fn blank() -> Self {
        Self { target: Self::ANY, flags: 0, subject: 0 }
    }
}

impl Default for MessageHeader {
    fn default() -> Self {
        Self::blank()       
    }
}

pub mod kernel {
    use crate::types::CTid;

    pub const EXCEPTION: u32 = 0xFFFFFFFF;
    pub const EXIT: u32 = 0xFFFFFFFE;
    pub const LOW_MEMORY: u32 = 0xFFFFFFFD;
    pub const OUT_OF_MEMORY: u32 = 0xFFFFFFFC;

    #[derive(Clone, Default, Hash)]
    #[repr(C, align(16))]
    pub struct ExceptionMessage {
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

    impl ExceptionMessage {
        // Error codes

        pub const PRESENT: u32 = 0x01;
        pub const WRITE: u32 = 0x02;
        pub const USER: u32 = 0x04;
        pub const RESD_WRITE: u32 = 0x08;
        pub const FETCH: u32 = 0x10;
    }

    impl From<[u8; 76]> for ExceptionMessage {
        fn from(value: [u8; 76]) -> Self {
            ExceptionMessage::try_from(&value[..]).unwrap()
        }
    }

    impl TryFrom<&[u8]> for ExceptionMessage {
        type Error = ();

        fn try_from(value: &[u8]) -> Result<Self, Self::Error> {
            if value.len() < core::mem::size_of::<ExceptionMessage>() {
                Err(())
            } else {
                Ok(Self {
                    eax: u32::from_le_bytes(*value[0..4].as_array().unwrap()),
                    ebx: u32::from_le_bytes(*value[4..8].as_array().unwrap()),
                    ecx: u32::from_le_bytes(*value[8..12].as_array().unwrap()),
                    edx: u32::from_le_bytes(*value[12..16].as_array().unwrap()),
                    esi: u32::from_le_bytes(*value[16..20].as_array().unwrap()),
                    edi: u32::from_le_bytes(*value[20..24].as_array().unwrap()),
                    ebp: u32::from_le_bytes(*value[24..28].as_array().unwrap()),
                    esp: u32::from_le_bytes(*value[28..32].as_array().unwrap()),
                    cs: u16::from_le_bytes(*value[32..34].as_array().unwrap()),
                    ds: u16::from_le_bytes(*value[34..36].as_array().unwrap()),
                    es: u16::from_le_bytes(*value[36..38].as_array().unwrap()),
                    fs: u16::from_le_bytes(*value[38..40].as_array().unwrap()),
                    gs: u16::from_le_bytes(*value[40..42].as_array().unwrap()),
                    ss: u16::from_le_bytes(*value[42..44].as_array().unwrap()),
                    eflags: u32::from_le_bytes(*value[44..48].as_array().unwrap()),
                    cr0: u32::from_le_bytes(*value[48..52].as_array().unwrap()),
                    cr2: u32::from_le_bytes(*value[52..56].as_array().unwrap()),
                    cr3: u32::from_le_bytes(*value[56..60].as_array().unwrap()),
                    cr4: u32::from_le_bytes(*value[60..64].as_array().unwrap()),
                    eip: u32::from_le_bytes(*value[64..68].as_array().unwrap()),
                    error_code: u32::from_le_bytes(*value[68..72].as_array().unwrap()),
                    fault_num: value[72],
                    processor_id: value[73],
                    who: CTid::from_le_bytes(*value[74..76].as_array().unwrap()),
                })
            }
        }
    }

    #[derive(Clone, Default, Hash)]
    #[repr(C, align(16))]
    pub struct ExitMessage {
        pub exit_code: i32,
        pub who: CTid,
    }

    impl From<[u8; 6]> for ExitMessage {
        fn from(value: [u8; 6]) -> Self {
            Self::try_from(&value[..]).unwrap()
        }
    }

    impl TryFrom<&[u8]> for ExitMessage {
        type Error = ();

        fn try_from(value: &[u8]) -> Result<Self, Self::Error> {
            if value.len() >= 6 {
                Ok(Self {
                    exit_code: i32::from_le_bytes(*value[0..4].as_array().unwrap()),
                    who: CTid::from_le_bytes(*value[4..6].as_array().unwrap()),
                })
            } else {
                Err(())
            }
        }
    }
}
