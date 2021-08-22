use crate::message::{self, Message, RawMessage};
use crate::message::init::{RawRegisterNameRequest, RegisterNameRequest, RawRegisterServerRequest, RegisterServerRequest};
use alloc::boxed::Box;
use crate::Tid;
use crate::error;
use alloc::prelude::v1::String;
use crate::syscall;

const DATA_BUF_SIZE: usize = 32;
const RAMDISK_MAJOR: i32 = 1;

fn handle_request(msg: Message<[u8; DATA_BUF_SIZE]>) -> Result<(), (error::Error, Option<String>)> {
    let raw_msg = msg.raw_message();

    match raw_msg.subject() {
        _ => Err((error::NOT_IMPLEMENTED, None))
    }
}

pub fn ramdisk_main() -> ! {
    eprintln!("Starting ramdisk...");
    let any_sender = Tid::new(RawMessage::ANY_SENDER);

    {
        let payload: Box<RawRegisterNameRequest> = Box::new(RegisterNameRequest {
            name: "ramdisk".bytes().collect()
        }.into());

        let mut register_message: Message<RawRegisterNameRequest> = Message::new(message::init::REGISTER_NAME, Some(payload), 0);

        match register_message.call::<()>(&Tid::new(syscall::INIT_TID)) {
            Err(code) => error::log_error(code, Some(String::from("Unable to register ramdisk name"))),
            Ok((msg, _)) => {
                if msg.subject == RawMessage::RESPONSE_OK {
                    eprintln!("Ramdisk registered successfully.");
                } else {
                    eprintln!("Unable to register ramdisk name.");
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

        match register_message.call::<()>(&Tid::new(syscall::INIT_TID)) {
            Err(code) => error::log_error(code, Some(String::from("Unable to register ramdisk name"))),
            Ok((msg, _)) => {
                if msg.subject == RawMessage::RESPONSE_OK {
                    eprintln!("Ramdisk registered successfully.");
                } else {
                    eprintln!("Unable to register ramdisk name.");
                }
            },
        }
    }

    loop {
        match message::receive::<[u8; DATA_BUF_SIZE]>(&any_sender, 0) {
            Ok((msg, _)) => {
                match handle_request(msg) {
                    Ok(_) => {},
                    Err((code, arg)) => error::log_error(code, arg),
                }
            },
            Err(code) => {
                eprintln!("Unable to receive message");
                error::log_error(code, None);
            }
        }
    }
}