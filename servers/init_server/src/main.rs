#![allow(dead_code)]
#![no_std]
#![no_main]
#![feature(alloc_error_handler)]
#![feature(lang_items)]
#![feature(asm)]

#[no_mangle]
pub extern "C" fn _start(multiboot_info: *mut RawMultibootInfo, last_free_page: PAddr) -> ! {
    lowlevel::print_debug("Initializing the init server...");
    lowlevel::print_debug("Multiboot info: 0x");
    lowlevel::print_debug_u32(multiboot_info as usize as u32, 16);
    lowlevel::print_debug(" Last free page: 0x");
    lowlevel::print_debug_u32(last_free_page as usize as u32, 16);
    lowlevel::print_debugln(".");
    let multiboot_box = init(multiboot_info, last_free_page);
    main(multiboot_box);
    syscall::exit(0)
}

#[macro_use] extern crate core;
#[macro_use] extern crate alloc;
extern crate num_traits;
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
mod ramdisk;

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
use alloc::boxed::Box;
use crate::syscall::c_types::ThreadInfo;
use crate::syscall::{ThreadStruct, INIT_TID};
use core::ffi::c_void;
use crate::address::VAddr;
use crate::device::DeviceId;
use crate::mapping::AddrSpace;
use crate::page::VirtualPage;
use crate::message::Message;
use core::convert::TryFrom;
use crate::message::init::{SimpleResponse, NameString};
use core::cmp::Ordering;
use alloc::prelude::v1::Vec;

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

impl PartialOrd for Tid {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        CTid::from(self).partial_cmp(&CTid::from(other))
    }
}

impl Ord for Tid {
    fn cmp(&self, other: &Self) -> Ordering {
        CTid::from(self).cmp(&CTid::from(other))
    }
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
    lowlevel::print_debugln("Initializing sbrk...");

    sbrk::init(last_free_kernel_page);
    lowlevel::print_debugln("Initializing page allocator...");

    PhysPageAllocator::init(multiboot_info, last_free_kernel_page);

    eprintln!("Initializing mapping manager...");
    mapping::manager::init();

    eprintln!("Initializing name manager...");
    name::manager::init();

    eprintln!("Initializing device manager...");
    device::manager::init();

    eprintln!("Initializing idle thread...");
    init_threads(vec![idle_main, ramdisk::ramdisk_main]);

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
                    elf::loader::load_init_mappings(&module);
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
                    .map_err(|_| (error::OPERATION_FAILED, Some(String::from("Failed to respond to kernel message."))))
                    .and_then(|ex_msg| {
                        let result = if ex_msg.int_num == 14 {
                            pager::handle_page_fault(&ex_msg)
                        } else {
                            error::dump_state(&message.sender);
                            Err((error::NOT_IMPLEMENTED, None))
                        };

                        let subject = if result.is_ok() {
                            RawMessage::RESPONSE_OK
                        } else {
                            RawMessage::RESPONSE_FAIL
                        };

                        let mut response: Message<()> = Message::new(subject, None, RawMessage::MSG_SYSTEM | RawMessage::MSG_NOBLOCK);

                        response.send(&message.sender)
                            .map_err(|code| (code, None))
                            .and(result)
                    })
            },
            kernel::EXIT => {
                ExitMessage::try_from(msg)
                    .map_err(|_| (error::OPERATION_FAILED, Some(String::from("Failed to respond to kernel message."))))
                    .and_then(|_exit_msg| {
                        let result = Ok(());

                        let subject = if result.is_ok() {
                            RawMessage::RESPONSE_OK
                        } else {
                            RawMessage::RESPONSE_FAIL
                        };

                        let mut response: Message<()> = Message::new(subject, None, RawMessage::MSG_SYSTEM | RawMessage::MSG_NOBLOCK);

                        response.send(&message.sender)
                            .map_err(|code| (code, None))
                            .and(result)
                    })
            },
            _ => {
                let result = Err((error::BAD_REQUEST, Some(format!("Kernel message with subject {} from TID {}", message.subject, msg.sender()))));
                let mut response: Message<()> = Message::new(RawMessage::RESPONSE_FAIL, None, RawMessage::MSG_SYSTEM | RawMessage::MSG_NOBLOCK);

                response.send(&message.sender)
                    .map_err(|code| (code, None))
                    .and(result)
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
                        eprintln!("Registering {}", new_name);
                        let register_result = name::manager::register(&new_name,
                                                                      message.sender.clone());
                        let mut response = RegisterNameResponse::new_message(message.sender.clone(),
                                                                         register_result.is_ok(),
                                                                             RawMessage::MSG_NOBLOCK);

                        message::send(&message.sender, &mut response)
                            .map(|_| ())
                    })
                    .map_err(|code| (error::OPERATION_FAILED, Some(format!("Failed to respond to request {} failed due to code: {}", message.subject, code))))
            },
            init::LOOKUP_NAME => {
                LookupNameRequest::try_from(msg)
                    .and_then(|request| {
                        let target_name = request.name_string()
                            .expect("A request with an invalid name was marked as valid.");
                        let tid_option = name::manager::lookup(&target_name)
                            .map(|tid_ref| tid_ref.clone());
                        let mut response = LookupNameResponse::new_message(message.sender.clone(),
                                                                           tid_option,
                                                                           RawMessage::MSG_NOBLOCK);

                        message::send(&message.sender, &mut response)
                            .map(|_| ())
                    })
                    .map_err(|code| (error::OPERATION_FAILED, Some(format!("Failed to respond to request {} failed due to code: {}", message.subject, code))))
            },
            init::UNREGISTER_NAME => {
                UnregisterNameRequest::try_from(msg)
                    .and_then(|request| {
                        let target_name = request.name_string()
                            .expect("A request with an invalid name was marked as valid.");
                        let tid_option = name::manager::unregister(&target_name);
                        let mut response = UnregisterNameResponse::new_message(message.sender.clone(),
                                                                               tid_option.is_some(),
                                                                               RawMessage::MSG_NOBLOCK);
                        message::send(&message.sender, &mut response)
                            .map(|_| ())
                    })
                    .map_err(|code| (error::OPERATION_FAILED, Some(format!("Failed to respond to request {} failed due to code: {}", message.subject, code))))
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
                                                                    addr_option,
                                                                    RawMessage::MSG_NOBLOCK);

                        message::send(&message.sender, &mut response)
                            .map(|_| ())
                    })
                    .map_err(|code| (error::OPERATION_FAILED, Some(format!("Failed to respond to request {} failed due to code: {}", message.subject, code))))
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
                        let mut response = UnmapResponse::new_message(message.sender.clone(),
                                                                      unmap_option.is_some(),
                                                                      RawMessage::MSG_NOBLOCK);

                        message::send(&message.sender, &mut response)
                            .map(|_| ())
                    })
                    .map_err(|code| (error::OPERATION_FAILED, Some(format!("Failed to respond to request {} failed due to code: {}", message.subject, code))))
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
                        let mut response = RegisterServerResponse::new_message(message.sender.clone(),
                                                                               true,
                                                                               RawMessage::MSG_NOBLOCK);

// TODO: This needs to be implemented

                        message::send(&message.sender, &mut response)
                            .map(|_| ())
                    })
                    .map_err(|code| (error::OPERATION_FAILED, Some(format!("Failed to respond to request {} failed due to code: {}", message.subject, code))))
            },
            init::UNREGISTER_SERVER => {
                Err((error::NOT_IMPLEMENTED, Some(format!("Request {}", msg.subject()))))
            },
            _ => Err((error::BAD_REQUEST, Some(format!("Request {:#x} sender: {} flags: {}", message.subject, msg.sender(), msg.flags())))),
        }
    }
}

fn idle_main() -> ! {
    loop {}
}

fn init_threads(thread_entries: Vec<fn() -> !>) {
    let init_tid = Tid::new(INIT_TID);
    let addr_space = mapping::manager::lookup_tid_mut(&init_tid)
        .expect("Unable to get the initial address space");
    let stack_size = 4096 * 1024;
    let pmap = syscall::get_init_pmap().unwrap().as_address();
    let zero_device = DeviceId::new_from_tuple((device::pseudo::MAJOR, device::pseudo::ZERO_MINOR));

    for (i, entry) in thread_entries.into_iter().enumerate() {
        let stack_top = 0xC0000000 - (i + 1) * stack_size;

        let tid = match unsafe {
            syscall::sys_create_thread(NULL_TID,
                                       entry as *const fn() -> ! as *const c_void,
                                       &pmap as *const PAddr,
                                       stack_top as *const c_void)
        } {
            NULL_TID => panic!("Unable to create new thread."),
            t => Tid::new(t)
        };

        addr_space.attach_thread(tid.clone());
        addr_space.map(Some((stack_top - stack_size + VirtualPage::SMALL_PAGE_SIZE) as VAddr),
                       &zero_device, 0, AddrSpace::EXTEND_DOWN | AddrSpace::NO_EXECUTE, stack_size - VirtualPage::SMALL_PAGE_SIZE);
        addr_space.map(Some((stack_top - stack_size) as VAddr),
                       &zero_device, 0, AddrSpace::GUARD | AddrSpace::NO_EXECUTE, VirtualPage::SMALL_PAGE_SIZE);

        let mut thread_info = ThreadInfo::default();
        let mut flags = ThreadInfo::STATUS;
        thread_info.status = ThreadInfo::READY;

        if entry == idle_main {
            thread_info.priority = 0;
            flags |= ThreadInfo::PRIORITY;
        };

        let thread_struct = ThreadStruct::new(thread_info, flags);
        match syscall::update_thread(&tid, &thread_struct) {
            Err(e) => error::log_error(e, Some(String::from("Error while initializing thread. Unable to start."))),
            _ => ()
        }
    }
}

fn main(_multiboot_info: Option<Box<MultibootInfo>>) {
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
