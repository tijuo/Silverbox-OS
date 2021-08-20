#![allow(dead_code)]

use hashbrown::HashMap;
use crate::mapping::MemoryRegion;
use crate::CTid;
use core::prelude::v1::*;
use alloc::vec::Vec;

pub type CPid = u16;
pub const NULL_PID: CPid = 0;

#[derive(PartialEq, Eq, Copy, Clone, Debug)]
pub struct Pid(Option<CPid>);

impl Pid {
    pub fn new(id: CPid) -> Pid {
        match id {
            NULL_PID => Pid(None),
            x => Pid(Some(x)),
        }
    }

    pub fn is_null(&self) -> bool {
        self.0.is_none()
    }

    pub fn null() -> CPid {
        NULL_PID
    }
}

impl From<CPid> for Pid {
    fn from(id: CPid) -> Self {
        Pid::new(id)
    }
}

impl From<Pid> for CPid {
    fn from(id: Pid) -> Self {
        match id.0 {
            None => NULL_PID,
            Some(x) => x,
        }
    }
}

#[derive(PartialEq, Eq, Copy, Clone)]
enum Status {
    Ready,
    Busy,
    Waiting,
}

struct Binding {
    buffer: MemoryRegion,
    status: Status,
}

struct AclRule {
    tid: CTid,
    permission: Permission,
}

impl PartialEq for AclRule {
    fn eq(&self, other: &AclRule) -> bool {
        self.tid == other.tid && self.permission == other.permission
    }
}

#[derive(PartialEq, Eq, Copy, Clone)]
enum Permission {
    SendOnly,
    ReceiveOnly,
    Any,
}

struct Port {
    creator: CTid,
    receivers: HashMap<CTid, Binding>,
    sender: Option<(CTid, Binding)>,
    flags: u32,
    acl_default: AclDefault,
    acl: Vec<AclRule>,
}

enum AclDefault {
    Whitelist,      // block all threads except those in acl
    Blacklist,      // allow all threads except those in acl
}

impl Port {
    fn new(creator: CTid, flags: u32) -> Port {
        Port {
            creator,
            flags,
            sender: None,
            receivers: HashMap::new(),
            acl_default: AclDefault::Whitelist,
            acl: Vec::new(),
        }
    }

    fn set_acl_default(&mut self, acl_default: AclDefault) {
        self.acl_default = acl_default;
    }

    fn add_acl_rule(&mut self, tid: CTid, permission: Permission) {
        self.acl.push(AclRule {
            tid,
            permission,
        });
    }

    fn remove_acl_rule(&mut self, tid: &CTid, permission: &Permission) -> bool {
        let rule = AclRule {
            tid: *tid,
            permission: *permission,
        };

        let mut rule_index = None;

        for (i, r) in self.acl.iter().enumerate() {
            if *r == rule {
                rule_index = Some(i);
                break;
            }
        }

        if let Some(index) = rule_index {
            self.acl.remove(index);
            true
        } else {
            false
        }
    }
}

#[cfg(test)]
mod test {
    use super::Pid;

    #[test]
    fn test_null() {
        assert_eq!(0, Pid::null());
    }

    #[test]
    fn test_is_null() {
        assert!(Pid::new(0).is_null());
        assert!(!Pid::new(7000).is_null());
        assert!(!Pid::new(28529).is_null());
    }
}