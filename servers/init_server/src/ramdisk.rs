use rust::message::MessageHeader;
use crate::message::init::{RawRegisterNameRequest, RegisterNameRequest, RawRegisterServerRequest, RegisterServerRequest};
use alloc::boxed::Box;
use crate::Tid;
use crate::error::Error;
use alloc::string::String;
use alloc::borrow::Cow;
use rust::syscalls;

const DATA_BUF_SIZE: usize = 32;
const RAMDISK_MAJOR: i32 = 1;

fn handle_request(header: MessageHeader, _data: &[u8]) -> Result<(), (Error, Option<String>)> {
    match header.subject {
        _ => Err((Error::NotImplemented, None))
    }
}

pub fn ramdisk_main() -> ! {
    eprintfln!("Starting ramdisk...");
    let any_sender = Tid::new(MessageHeader::ANY_SENDER);

    {
        let payload: Box<RawRegisterNameRequest> = Box::new(RegisterNameRequest {
            name: "ramdisk".bytes().collect()
        }.into());

        let mut register_message: Message<RawRegisterNameRequest> = Message::new(message::init::REGISTER_NAME, Some(payload), 0);

        match register_message.call::<()>(&Tid::new(syscalls::INIT_TID)) {
            Err(code) => error::log_error(code, Cow::Borrowed("Unable to register ramdisk name")),
            Ok((msg, _)) => {
                if msg.subject == RawMessage::RESPONSE_OK {
                    eprintfln!("Ramdisk name registered successfully.");
                } else {
                    eprintfln!("Unable to register ramdisk name.");
                }
            },
        }
    }

    {
        let payload: Box<RawRegisterServerRequest> = Box::new(RegisterServerRequest {
            server_type: RawRegisterServerRequest::DEVICE_DRIVER,
            id: RAMDISK_MAJOR,
        }.into());

        let mut register_message: Message<RawRegisterServerRequest> = Message::new(message::init::REGISTER_SERVER, Some(payload), 0);

        match register_message.call::<()>(&Tid::new(syscalls::INIT_TID)) {
            Err(code) => error::log_error(code, Some(String::from("Unable to register ramdisk name"))),
            Ok((msg, _)) => {
                if msg.subject == RawMessage::RESPONSE_OK {
                    eprintfln!("Ramdisk server registered successfully.");
                } else {
                    eprintfln!("Unable to register ramdisk name.");
                }
            },
        }
    }

    loop {
        match syscalls::receive(&any_sender, 0) {
            Ok((msg, _)) => {
                match handle_request(msg) {
                    Ok(_) => {},
                    Err((code, arg)) => error::log_error(code, arg),
                }
            },
            Err(code) => {
                eprintfln!("Unable to receive message");
                error::log_error(code, None);
            }
        }
    }
}