#![allow(dead_code)]
#![no_std]
#![no_main]
#![feature(alloc_error_handler)]
#![feature(lang_items)]
#![feature(asm)]

#[no_mangle]
pub extern "C" fn _start(multiboot_info: *mut RawMultibootInfo, last_free_page: PAddr) -> ! {
    print_debug("Multiboot info: 0x");
    print_debug_u32(multiboot_info as usize as u32, 16);
    print_debug(" Last free page: 0x");
    print_debug_u32(last_free_page as usize as u32, 16);
    print_debugln(".");
    let multiboot_box = init(multiboot_info, last_free_page);
    main(multiboot_box);
    syscall::exit(0)
}

#[macro_use] extern crate core;
#[macro_use] extern crate alloc;
extern crate num_traits;
extern crate hashbrown;
#[macro_use] extern crate memoffset;

mod pager;
mod address;
mod name;
mod syscall;
mod port;
mod message;
mod page;
mod mapping;
mod error;
mod device;
#[macro_use]
mod lowlevel;
mod sbrk;
#[allow(dead_code)]
mod elf;

use address::{PAddr, PSize};
use message::RawMessage;
use message::{init, kernel};
use crate::message::init::{RegisterNameRequest, LookupNameRequest, UnregisterNameRequest,
                           MapResponse, UnmapResponse, MapRequest, UnmapRequest, LookupNameResponse,
                           RegisterNameResponse, UnregisterNameResponse, RegisterServerRequest,
                           RegisterServerResponse};
use crate::message::kernel::{ExceptionMessage, ExitMessage};
use syscall::c_types::{CTid, NULL_TID};
use core::prelude::v1::*;
use alloc::string::String;
use crate::pager::page_alloc::PhysPageAllocator;
use crate::elf::{RawMultibootInfo, MultibootInfo};
use crate::lowlevel::{print_debug, print_debug_u32, print_debugln};
use alloc::boxed::Box;
use crate::syscall::c_types::ThreadInfo;
use crate::syscall::{ThreadStruct, get_init_pmap, INIT_TID};
use core::ffi::c_void;
use crate::address::VAddr;
use crate::device::DeviceId;
use crate::mapping::AddrSpace;
use crate::page::VirtualPage;
use crate::elf::loader::load_init_mappings;
use crate::message::{Message, BlankMessage};
use core::convert::TryFrom;
use crate::error::{BAD_REQUEST, NOT_IMPLEMENTED};
use crate::message::init::{SimpleResponse, NameString};
const DATA_BUF_SIZE: usize = 64;

#[derive(PartialEq, Eq, Copy, Clone, Debug, Hash)]
pub struct Tid(Option<CTid>);

impl Tid {
    pub fn new(id: CTid) -> Tid {
        match id {
            NULL_TID => Tid(None),
            x => Tid(Some(x)),
        }
    }

    pub fn is_null(&self) -> bool {
        self.0.is_none()
    }

    pub fn null_ctid() -> CTid {
        NULL_TID
    }
    pub fn null() -> Tid { Tid(None) }
}

impl From<CTid> for Tid {
    fn from(id: CTid) -> Self {
       Tid::new(id)
    }
}

impl From<&CTid> for Tid {
    fn from(id: &CTid) -> Self {
        Tid::new(*id)
    }
}

impl From<Tid> for CTid {
    fn from(id: Tid) -> Self {
        match id.0 {
            None => NULL_TID,
            Some(x) => x,
        }
    }
}

impl From<&Tid> for CTid {
    fn from(id: &Tid) -> Self {
        match id.0 {
            None => NULL_TID,
            Some(x) => x,
        }
    }
}

impl Default for Tid {
    fn default() -> Self {
        Tid::null()
    }
}

fn init(multiboot_info: * const RawMultibootInfo, last_free_kernel_page: PAddr) -> Option<Box<MultibootInfo>> {
    print_debugln("Initializing sbrk...");

    sbrk::init(last_free_kernel_page);
    print_debugln("Initializing page allocator...");

    PhysPageAllocator::init(multiboot_info, last_free_kernel_page);

    eprintln!("Initializing mapping manager...");
    mapping::manager::init();

    eprintln!("Initializing name manager...");
    name::manager::init();

    eprintln!("Initializing device manager...");
    device::manager::init();

    eprintln!("Initializing idle thread...");
    init_idle_thread();

    eprintln!("Loading modules...");
    let multiboot_box = elf::read_multiboot_info(multiboot_info as usize as PAddr);

    load_modules(&multiboot_box);

    multiboot_box
}

fn load_modules(multiboot_info: &Option<Box<MultibootInfo>>) {
    if let Some(ref mb_info) = multiboot_info {
        let initsrv_name = mb_info.command_line.as_ref()
            .and_then(|s| {
                s.split_once("initsrv=")
                    .map(|(_, suffix)| String::from(suffix.clone()))
            });

        if let Some(ref modules) = mb_info.modules {
            for module in modules {
                if initsrv_name.is_some() && module.name.is_some()
                    && initsrv_name.as_ref().unwrap() == module.name.as_ref().unwrap() {
                    load_init_mappings(&module);
                } else {
                    match elf::loader::load_module(&module) {
                      Err(_) => eprintln!("Unable to load module @ {:#x}", module.addr),
                      _ => (),
                    };
                }
            }
        }
    } else {
        eprintln!("Multiboot info is missing.");
    }
}

fn handle_message<T>(message: Message<T>) -> Result<(), (error::Error, Option<String>)> {
    let msg = message.raw_message();

    if msg.is_flags_set(RawMessage::MSG_KERNEL) {
        let result = match msg.subject() {
            kernel::EXCEPTION => {
                ExceptionMessage::try_from(msg)
                    .map_err(|_| (BAD_REQUEST, Some(String::from("Request message is malformed"))))
                    .and_then(|ex_msg| {
                        let result = if ex_msg.int_num == 14 {
                            pager::handle_page_fault(&ex_msg)
                        } else {
                            error::dump_state(&message.sender);
                            Err((NOT_IMPLEMENTED, None))
                        };

                        let mut response = BlankMessage::new_blank();
                        let subject = if result.is_ok() {
                            RawMessage::RESPONSE_OK
                        } else {
                            RawMessage::RESPONSE_FAIL
                        };

                        response.send(&message.sender,
                                      subject,
                                      RawMessage::MSG_SYSTEM | RawMessage::MSG_NOBLOCK);

                        result
                    })
            },
            kernel::EXIT => {
                ExitMessage::try_from(msg)
                    .and_then(|_exit_msg| {
                        let mut response = BlankMessage::new_blank();
                        let subject = RawMessage::RESPONSE_OK;

                        response.send(&message.sender,
                                      subject,
                                      RawMessage::MSG_SYSTEM | RawMessage::MSG_NOBLOCK);
                        Ok(())
                    })
                    .map_err(|_| (BAD_REQUEST, Some(String::from("Request message is malformed"))))
            },
            _ => {
                let mut response = BlankMessage::new_blank();
                let subject = RawMessage::RESPONSE_FAIL;

                response.send(&message.sender,
                              subject,
                              RawMessage::MSG_SYSTEM | RawMessage::MSG_NOBLOCK);

                Err((error::BAD_REQUEST, Some(format!("Kernel message with subject {} from TID {}", msg.subject(), msg.sender()))))
            },
        };

        result
    }
    else {
        match msg.subject() {
            init::REGISTER_NAME => {
                RegisterNameRequest::try_from(msg)
                    .and_then(|request| {
                        let new_name = request.name_string()
                            .expect("A request with an invalid name was marked as valid.");
                        let register_result = name::manager::register(&new_name,
                                                                      message.sender.clone());
                        let mut response = RegisterNameResponse::new_message(message.sender.clone(),
                                                                         register_result.is_ok());

                        message::send(&message.sender, &mut response, RawMessage::MSG_NOBLOCK)
                            .map(|_| ())
                    })
                    .map_err(|_| (BAD_REQUEST, Some(String::from("Request message is malformed"))))
            },
            init::LOOKUP_NAME => {
                LookupNameRequest::try_from(msg)
                    .and_then(|request| {
                        let target_name = request.name_string()
                            .expect("A request with an invalid name was marked as valid.");
                        let tid_option = name::manager::lookup(&target_name)
                            .map(|tid_ref| tid_ref.clone());
                        let mut response = LookupNameResponse::new_message(message.sender.clone(),
                                                                           tid_option);

                        message::send(&message.sender, &mut response, RawMessage::MSG_NOBLOCK)
                            .map(|_| ())
                    })
                    .map_err(|code| (code, None))
            },
            init::UNREGISTER_NAME => {
                UnregisterNameRequest::try_from(msg)
                    .and_then(|request| {
                        let target_name = request.name_string()
                            .expect("A request with an invalid name was marked as valid.");
                        let tid_option = name::manager::unregister(&target_name);
                        let mut response = UnregisterNameResponse::new_message(message.sender.clone(),
                                                                               tid_option.is_some());
                        message::send(&message.sender, &mut response, RawMessage::MSG_NOBLOCK)
                            .map(|_| ())
                    })
                    .map_err(|code| (code, None))
            },
            init::MAP => {
                MapRequest::try_from(msg)
                    .and_then(|request| {
                        let addr_option = mapping::manager::lookup_tid_mut(&message.sender)
                            .and_then(|addr_space|
                                addr_space.map(request.address,
                                               &request.device,
                                               request.offset,
                                               request.flags,
                                               request.length));

                        let mut response = MapResponse::new_message(message.sender.clone(),
                                                                    addr_option);

                        message::send(&message.sender, &mut response, RawMessage::MSG_NOBLOCK)
                            .map(|_| ())
                    })
                    .map_err(|code| (code, None))
            },
            init::UNMAP => {
                UnmapRequest::try_from(msg)
                    .and_then(|request| {
                        let unmap_option = mapping::manager::lookup_tid_mut(&message.sender)
                            .and_then(|addr_space|
                                match addr_space.unmap(request.address, request.length) {
                                    true => Some(()),
                                    false => None,
                                });
                        let mut response = UnmapResponse::new_message(message.sender.clone(), unmap_option.is_some());

                        message::send(&message.sender, &mut response, RawMessage::MSG_NOBLOCK)
                            .map(|_| ())
                    })
                    .map_err(|code| (code, None))
            },
            init::CREATE_PORT => {
                Err((error::NOT_IMPLEMENTED, Some(format!("Request {}", msg.subject()))))
            },
            init::SEND_PORT => {
                Err((error::NOT_IMPLEMENTED, Some(format!("Request {}", msg.subject()))))
            },
            init::RECEIVE_PORT => {
                Err((error::NOT_IMPLEMENTED, Some(format!("Request {}", msg.subject()))))
            },
            init::DESTROY_PORT => {
                Err((error::NOT_IMPLEMENTED, Some(format!("Request {}", msg.subject()))))
            },
            init::REGISTER_SERVER => {
                RegisterServerRequest::try_from(msg)
                    .and_then(|_request| {
                        let mut response = RegisterServerResponse::new_message(message.sender.clone(), true);

// TODO: This needs to be impemented

                        message::send(&message.sender, &mut response, RawMessage::MSG_NOBLOCK)
                            .map(|_| ())
                    })
                    .map_err(|code| (code, None))
            },
            init::UNREGISTER_SERVER => {
                Err((error::NOT_IMPLEMENTED, Some(format!("Request {}", msg.subject()))))
            },
            _ => Err((error::BAD_REQUEST, Some(format!("Request {:#x} sender: {} flags: {}", msg.subject(), msg.sender(), msg.flags())))),
        }
    }
}

fn do_nothing() -> ! {
    loop {}
}

fn init_idle_thread() {
    let pmap = get_init_pmap().unwrap().as_address();
    let stack_top = 0xC0000000 - 64 * 1024;
    let zero_device = DeviceId::new_from_tuple((device::pseudo::MAJOR, device::pseudo::ZERO_MINOR));
    let stack_size = 4096;

    let tid = unsafe {
        syscall::sys_create_thread(NULL_TID,
                                   do_nothing as *const c_void,
                                   &pmap as *const PAddr,
                                   stack_top as *const c_void)
    };

    if tid == NULL_TID {
        panic!("Unable to create idle thread");
    }

    let tid = Tid::new(tid);
    let init_tid = Tid::new(INIT_TID);
    let addr_space = mapping::manager::lookup_tid_mut(&init_tid)
        .expect("Unable to get the initial address space");
    addr_space.attach_thread(tid.clone());
    addr_space.map(Some((stack_top - stack_size) as VAddr),
                   &zero_device, 0, AddrSpace::EXTEND_DOWN | AddrSpace::NO_EXECUTE, stack_size);
    addr_space.map(Some((stack_top - stack_size - VirtualPage::SMALL_PAGE_SIZE) as VAddr),
                   &zero_device, 0, AddrSpace::GUARD | AddrSpace::NO_EXECUTE, VirtualPage::SMALL_PAGE_SIZE);

    let mut thread_info = ThreadInfo::default();
    thread_info.status = ThreadInfo::READY;
    thread_info.priority = 0;
    let thread_struct = ThreadStruct::new(thread_info, ThreadInfo::STATUS | ThreadInfo::PRIORITY);
    syscall::update_thread(&tid, &thread_struct);
}

fn main(_multiboot_info: Option<Box<MultibootInfo>>) {
    eprintln!("Initializing the init server...");
    let any_sender = Tid::new(RawMessage::ANY_SENDER);

    loop {

        match message::receive::<[u8; DATA_BUF_SIZE]>(&any_sender, 0) {
            Ok((msg, _)) => {
                match handle_message(msg) {
                    Ok(_) => {},
                    Err((code, arg)) => error::log_error(code, arg),
                }
            },
            Err(code) => {
                error::log_error(code, None);
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::Tid;

    #[test]
    fn test_null() {
        assert_eq!(0, Tid::null_ctid());
    }

    #[test]
    fn test_is_null() {
        assert!(Tid::new(0).is_null());
        assert!(!Tid::new(7000).is_null());
        assert!(!Tid::new(28529).is_null());
    }
}
