#![allow(dead_code)]
#![no_std]
#![no_main]
#![feature(alloc_error_handler)]

extern "C" {
    fn sys_exit(code: i32) -> !;
}

#[no_mangle]
pub extern "C" fn _start(multiboot_info: *mut RawMultibootInfo, first_free_page: PAddr, stack_size: usize, _fxsave_state: [u8; 512]) -> ! {
    eprintfln!("Starting system.");
    eprintfln!("Initializing the init server...");
    eprintfln!("Multiboot info: {:#x}", multiboot_info as usize);
    eprintfln!("First free page: {:#x}", first_free_page as usize);
    eprintfln!("Stack size: {} bytes", stack_size);
    let multiboot_box = init(multiboot_info as *const RawMultibootInfo,
                             first_free_page, stack_size);
    main(multiboot_box);

    unsafe {
        sys_exit(1);
    }
}

#[macro_use] extern crate core;
#[macro_use] extern crate alloc;
extern crate num_traits;
#[macro_use] extern crate rust;

mod pager;
mod address;
mod name;
mod port;
mod page;
mod mapping;
mod error;
mod device;
#[macro_use] mod lowlevel;
mod sbrk;
mod elf;
mod ramdisk;
mod thread;
mod multiboot;
mod region;
mod phys_alloc;
mod vfs;
mod fat;
mod mutex;
mod message;

use address::PAddr;
use crate::multiboot::{RawMultibootInfo, MultibootInfo};
use alloc::boxed::Box;
use crate::phys_alloc::PhysPageAllocator;
use rust::types::CTid;
use rust::message::kernel::{self, ExceptionMessage, ExitMessage};
use rust::message::MessageHeader;
use rust::syscalls;
use rust::syscalls::PageMapping;
use crate::page::PhysicalFrame;
use alloc::borrow::Cow;
use crate::error::Error;
use crate::Tid;

const DATA_BUF_SIZE: usize = 64;

fn init(multiboot_info: *const RawMultibootInfo, first_free_page: PAddr, _stack_size: usize) -> Option<Box<MultibootInfo>> {
    eprintfln!("Initializing bootstrap allocator");

    PhysPageAllocator::init_bootstrap(first_free_page);

    // This seems to map PDEs for the first 3 GiB of memory
    // FIXME: This code is supposed to be temporary

    
    for ptab_num in 0..=768 {
        let vaddr = (ptab_num * PhysicalFrame::SMALL_PAGE_SIZE * 1024) as *mut ();
        let mut page_mapping = [PageMapping {
            number: 0,
            flags: 0
        }];

        syscalls::get_page_mappings(1, vaddr, None, &mut page_mapping)
            .expect("Unable to retrieve page mapping.");

        if is_flag_set!(page_mapping[0].flags, syscalls::flags::mapping::UNMAPPED) {
            /*let phys_frame = PhysicalFrame::new(phys_alloc::alloc_phys(Block4k)
                                               .expect("Unable to allocate memory for page table.")
                                               .0, FrameSize::Small);

            if !phys_frame.is_valid_pmap_base() {
                panic!("Physical allocator did not return a valid page table.");
            }*/

            page_mapping[0].flags = syscalls::flags::mapping::CLEAR;
            page_mapping[0].number = (0x4000000+ptab_num*4096) as u32 / 4096; //phys_frame.frame() as u32;

            syscalls::set_page_mappings(1, vaddr, None, &page_mapping)
                .expect("Unable to set page mapping.");
        }
    }
    

    let mmap_iter = unsafe {
        RawMultibootInfo::raw_mmap_iter(multiboot_info as usize as PAddr)
    }.expect("Unable to read memory map from Multiboot info");

    mmap_iter.clone()
        .for_each(|mmap| {
            eprintfln!("Mmap start: {:#x}:{:#x} type: {}", mmap.base_addr, mmap.base_addr + mmap.length, mmap.map_type);
        });


    eprintfln!("Initializing sbrk...");
    sbrk::init();
    eprintfln!("Initializing page allocator...");

    phys_alloc::PhysPageAllocator::init(mmap_iter);

    eprintfln!("Initializing mapping manager...");
    mapping::manager::init();

    eprintfln!("Initializing name manager...");
    name::manager::init();

    eprintfln!("Initializing device manager...");
    device::manager::init();

    eprintfln!("Initializing idle thread...");
    init_threads(vec![idle_main, ramdisk::ramdisk_main]);

    eprintfln!("Loading modules...");

    let multiboot_box = unsafe {
        MultibootInfo::from_phys(multiboot_info as usize as PAddr)
    };

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
                      Err(_) => eprintfln!("Unable to load module @ {:#x}", module.addr),
                      _ => (),
                    };
                }
            }
        }
    } else {
        eprintfln!("Multiboot info is missing.");
    }
}


fn handle_message(header: &mut MessageHeader, recv_buffer: &mut [u8]) -> Result<(), (Error, Cow<'static, str>)> {
    let message_sender = header.target.expect("Message should have a sender");

    if rust::is_flag_set!(header.flags, MessageHeader::MSG_KERNEL) {
        let result = match header.subject {
            kernel::EXCEPTION => {
                ExceptionMessage::try_from(&recv_buffer[..])
                    .map_err(|_| (Error::ParseError, Cow::Borrowed("Unable to read exception message.")))
                    .and_then(|payload| {
                        if payload.fault_num == 14 {
                            pager::handle_page_fault(&payload)

                            /*
                                                      let mut thread_info = ThreadInfo::default();
                            thread_info.status = ThreadInfo::READY;

                            syscalls::update_thread(&Tid::from(ex_msg.who),
                                                   &ThreadStruct::new(thread_info, ThreadInfo::STATUS))
                                .map_err(|e| (e, None))
                             */
                        } else {
                            error::dump_state(&payload);
                            Err((Error::NotImplemented, Cow::Borrowed("Not implemented")))
                        }
                    })
            },
            kernel::EXIT => {
                ExitMessage::try_from(&recv_buffer[..])
                    .map_err(|_| (Error::ParseError, Cow::Borrowed("Unable to read exit message.")))
                    .and_then(|_payload|
                        // TODO: Notify threads that are waiting to join with the stopped thread

                        Ok(()))
            },
            _ => {
                Err((Error::BadRequest,
                                  Cow::Owned(format!("Kernel message with subject {} from TID {}",
                                               header.subject,
                                               CTid::from(header.target
                                                   .expect("Message is supposed to have a sender"))))))
            },
        };

        result
    }
    else {
        match header.subject {
            
            init::REGISTER_NAME => {
                RegisterNameRequest::try_from(msg)
                    .and_then(|request| {
                        let new_name = request.name_string()
                            .expect("A request with an invalid name was marked as valid.");
                        eprintfln!("Registering {}", new_name);
                        let register_result = name::manager::register(&new_name,
                                                                      message.sender.clone());
                        let mut response = RegisterNameResponse::new_message(message.sender.clone(),
                                                                         register_result.is_ok(),
                                                                             RawMessage::MSG_NOBLOCK);

                        message::send(&message.sender, &mut response)
                            .map(|_| ())
                    })
                    .map_err(|code| (Error::Failed, Some(format!("Failed to respond to request {} failed due to code: {}", message.subject, code))))
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
                    .map_err(|code| (Error::Failed, Some(format!("Failed to respond to request {} failed due to code: {}", message.subject, code))))
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
                    .map_err(|code| (Error::Failed, Some(format!("Failed to respond to request {} failed due to code: {}", message.subject, code))))
            },
            init::MAP => {
                MapRequest::try_from(msg)
                    .and_then(|request| {
                        let addr_option = mapping::manager::lookup_tid_mut(message_sender)
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
                    .map_err(|code| (Error::Failed, Some(format!("Failed to respond to request {} failed due to code: {}", message.subject, code))))
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
                    .map_err(|code| (Error::Failed, Some(format!("Failed to respond to request {} failed due to code: {}", message.subject, code))))
            },

            init::CREATE_PORT => {
                Err((Error::NotImplemented, Some(format!("Request {}", msg.subject()))))
            },
            init::SEND_PORT => {
                Err((Error::NotImplemented, Some(format!("Request {}", msg.subject()))))
            },
            init::RECEIVE_PORT => {
                Err((Error::NotImplemented, Some(format!("Request {}", msg.subject()))))
            },
            init::DESTROY_PORT => {
                Err((Error::NotImplemented, Some(format!("Request {}", msg.subject()))))
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
                    .map_err(|code| (Error::Failed, Some(format!("Failed to respond to request {} failed due to code: {}", message.subject, code))))
            },
            init::UNREGISTER_SERVER => {
                Err((Error::NotImplemented, Some(format!("Request {}", msg.subject()))))
            },
            _ => Err((Error::BadRequest, Cow::Owned(format!("Request {:#x} sender: {} flags: {}",
                                                       header.subject,
                                                       CTid::from(message_sender),
                                                       header.flags)))),
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
    let pmap = syscalls::get_init_pmap().unwrap().as_address();
    let zero_device = DeviceId::new_from_tuple((device::pseudo::MAJOR, device::pseudo::ZERO_MINOR));

    for (i, entry) in thread_entries.into_iter().enumerate() {
        let stack_top = 0xF8000000 - (i + 1) * stack_size;

        let tid =
            syscalls::create_thread(entry as *const fn() -> ! as *const c_void,
                                       pmap as u32,
                                       stack_top as *const c_void).expect("Unable to create new thread.");

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
        match syscalls::update_thread(&tid, &thread_struct) {
            Err(e) => error::log_error(e, Some(String::from("Error while initializing thread. Unable to start."))),
            _ => ()
        }
    }
}

fn main(_multiboot_info: Option<Box<MultibootInfo>>) {
    let mut header = MessageHeader::blank();
    let mut recv_buffer= [0; 1024];

    loop {
        match syscalls::receive(&mut header, &mut recv_buffer)
            .map_err(|e| match e {
                syscalls::SyscallError::Interrupted => {
                    (Error::NotImplemented, Cow::Borrowed("Handling interrupted receives aren't implemented yet"))
                },
                syscalls::SyscallError::Preempted => {
                    (Error::NotImplemented, Cow::Borrowed("Handling kernel message preemption isn't implemented yet"))
                }
                _ => (Error::Failed, Cow::Borrowed("Receive failed")),
            })
            .and_then(|msg_len| handle_message(&mut header, &mut recv_buffer[..msg_len])) {
            Err((e, msg)) => error::log_error(e, msg),
            _ => (),
        }
    }
}
