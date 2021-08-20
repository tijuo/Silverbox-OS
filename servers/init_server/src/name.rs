static MAX_NAME_LEN: usize = 12;

pub(crate) mod manager {
    use crate::Tid;
    use hashbrown::HashMap;
    use crate::error::{ALREADY_REGISTERED, LONG_NAME};
    use super::MAX_NAME_LEN;
    use alloc::string::String;
    use alloc::vec::Vec;

    static mut NAME_MAP: Option<HashMap<String, Tid>> = None;

    /// Initialize the name map. This should only be called once.

    pub fn init() {
        unsafe {
            NAME_MAP = match NAME_MAP {
                Some(_) => panic!("Name map has already been initialized."),
                None => Some(HashMap::new()),
            }
        }
    }

    fn name_map() -> &'static HashMap<String, Tid> {
        unsafe {
            NAME_MAP.as_ref()
                .expect("Name map hasn't been initialized yet.")
        }
    }

    fn name_map_mut() -> &'static mut HashMap<String, Tid> {
        unsafe {
            NAME_MAP.as_mut()
                .expect("Name map hasn't been initialized yet.")
        }
    }

    /// Associate a name with a thread id.
    ///
    /// Returns `Ok(())` on success. Otherwise, it returns `Err()` if the name is
    /// too long or if the name is already registered.
    ///
    /// # Examples
    ///
    /// ```
    /// # use crate::name::manager::init;
    /// use crate::name::Tid;
    /// use crate::name::manager::{register, lookup};
    ///
    /// # fn main() {
    /// # init();
    /// let tid = Tid::new(14);
    ///
    /// assert_eq!(lookup("pager"), &None);
    /// register("pager", tid);
    /// assert_eq!(lookup("pager"), &Some(Tid::new(14)));
    /// # }
    /// ```

    pub fn register(name: &str, id: Tid) -> Result<(), i32> {
        let nm = name_map_mut();

        if name.len() > MAX_NAME_LEN {
            Err(LONG_NAME)
        } else if nm.contains_key(name) {
            Err(ALREADY_REGISTERED)
        } else {
            nm.insert(String::from(name), id);
            Ok(())
        }
    }

    /// Retrieve the thread id that's registered to a name, if it exists.
    ///
    /// # Examples
    ///
    /// ```
    /// # use crate::name::manager::init;
    /// use crate::name::Tid;
    /// use crate::name::manager::{register, lookup};
    ///
    /// # fn main() {
    /// # init();
    /// register("init", Tid::new(1));
    /// register("rtc", Tid::new(1001));
    ///
    /// assert_eq!(lookup("rtc"), &Some(Tid::new(1001)));
    /// assert_eq!(lookup("device"), &None);
    /// # }
    /// ```

    #[inline]
    pub fn lookup<'a, 'b>(name: &'a str) -> Option<&'b Tid> {
        name_map().get(name)
    }

    /// Remove a name that has been registered to a TID from the name map and
    /// return the previously registered TID if it was previously in the map.
    ///
    /// # Examples
    ///
    /// ```
    /// # use crate::name::manager::init;
    /// use crate::name::Tid;
    /// use crate::name::manager::{register, lookup, unregister};
    ///
    /// # fn main() {
    /// # init();
    /// let time_tid = Tid::new(101);
    ///
    /// register("time", time_tid.clone());
    /// register("init", Tid::new(1));
    ///
    /// assert_eq!(lookup("time"), &Some(time_tid.clone()));
    /// assert_eq!(unregister("time"), &Some(time_tid.clone()));
    /// assert_eq!(lookup("time"), &None);
    /// # }
    /// ```

    #[inline]
    pub fn unregister(name: &str) -> Option<Tid> {
        name_map_mut().remove(name)
    }

    /// Removes all names that are registered to a TID and returns the previously
    /// registered names, if any.
    ///
    /// # Examples
    ///
    /// ```
    /// # use crate::name::manager::init;
    /// use crate::name::Tid;
    /// use crate::name::manager::{register, unregister_tid};
    ///
    /// # fn main() {
    /// #    init();
    /// let time_tid = Tid::new(101);
    /// let init_tid = Tid::new(1);
    ///
    /// register("time", time_tid.clone());
    /// register("init", init_tid.clone());
    /// register("clock", time_tid.clone());
    ///
    /// let old_names = unregister_tid(&time_tid);
    ///
    /// assert!(old_names.contains("time"));
    /// assert!(!old_names.contains("init"));
    ///
    /// assert_eq!(lookup("init"), &Some(init_tid.clone()));
    /// assert_eq!(lookup("clock"), &None);
    /// # }
    /// ```

    pub fn unregister_tid(id: &Tid) -> Option<Vec<String>> {
        let name_map = name_map_mut();

        let matched_keys: Vec<String> = name_map
            .iter()
            .filter_map(|(k, v)| {
                if *v == *id {
                    Some(k.clone())
                } else {
                    None
                }
            })
            .collect();

        name_map.retain(|_,v| *v != *id);

        if matched_keys.is_empty() {
            None
        } else {
            Some(matched_keys)
        }
    }
}

#[cfg(test)]
mod test {
    use hashbrown::HashMap;
    use crate::Tid;
    use super::manager::{init, register, lookup, unregister, unregister_tid};
    use alloc::string::String;
    
    #[test]
    fn test_register() {

        init();
        
        assert!(register("initsrv", Tid::new(1024)).is_ok());
        assert!(register("rtc", Tid::new(1025)).is_ok());
        assert!(register("vfs", Tid::new(1026)).is_ok());
        assert!(register("send", Tid::new(1)).is_ok());
        assert!(register("receive", Tid::new(2)).is_ok());
        assert!(register("time", Tid::new(3)).is_ok());
        assert!(register("", Tid::new(94)).is_ok());
        assert!(register("my app", Tid::new(1)).is_ok());
        assert!(register("a really really really long name", Tid::new(27)).is_err());
        assert!(register("send", Tid::new(2222)).is_err());
        assert!(register("rtc", Tid::new(1025)).is_err());
    }

    #[test]
    fn test_lookup() {
        init();
        
        assert!(lookup("initsrv").is_none());
        assert!(lookup("").is_none());

        register("initsrv", Tid::new(1024)).unwrap();
        register("rtc", Tid::new(1025)).unwrap();
        register("vfs", Tid::new(1026)).unwrap();

        {
            let option1 = lookup("initsrv");

            assert!(option1.is_some());
            assert_eq!(option1.unwrap(), &Tid::new(1024));
        }

        assert!(lookup("").is_none());

        {
            let option2 = lookup("vfs");

            assert!(option2.is_some());

            assert_eq!(option2.unwrap(), &Tid::new(1026));
        }

        assert_ne!(lookup("rtc").unwrap(), &Tid::new(1024));
    }

    #[test]
    fn test_unregister() {
        init();
        
        register("initsrv", Tid::new(1))
            .expect("Unable to register initsrv.");
        register("rtc", Tid::new(2))
            .expect("Unable to register rtc.");
        register("vfs", Tid::new(3))
            .expect("Unable to register vfs.");
        register("send", Tid::new(2))
            .expect("Unable to register send.");

        assert!(lookup("rtc").is_some());

        {
            let option = unregister("rtc");

            assert_eq!(&option, &Some(Tid::new(2)));
        }

        assert!(lookup("rtc").is_none());
        assert!(lookup("send").is_some());
    }

    #[test]
    fn test_contains_key() {
        init();

        assert!(register("initsrv", Tid::new(1024)).is_ok());
        assert!(register("rtc", Tid::new(1025)).is_ok());
        assert!(register("vfs", Tid::new(1026)).is_ok());
        assert!(register("send", Tid::new(1)).is_ok());
        assert!(register("receive", Tid::new(2)).is_ok());
        assert!(register("time", Tid::new(3)).is_ok());
        assert!(register("", Tid::new(94)).is_ok());
        assert!(register("my app", Tid::new(1)).is_ok());
        assert!(register("a really really really long name", Tid::new(27)).is_err());
        assert!(register("send", Tid::new(2222)).is_err());
        assert!(register("rtc", Tid::new(1025)).is_err());

        assert!(lookup("initsrv").is_some());
        assert!(lookup("send").is_some());
        assert!(lookup("hello").is_none());
        assert!(lookup("time").is_some());
        assert!(lookup("vfs").is_some());
        assert!(lookup("blah").is_none());

    }

    #[test]
    fn test_unregister_tid() {
        init();

        register("initsrv", Tid::new(1))
            .expect("Unable to register initsrv.");
        register("rtc", Tid::new(2))
            .expect("Unable to register rtc.");
        register("vfs", Tid::new(3))
            .expect("Unable to register vfs.");
        register("send", Tid::new(2))
            .expect("Unable to register send.");
        register("transmitter", Tid::new(1))
            .expect("Unable to register transmitter.");
        register("receptor", Tid::new(4))
            .expect("Unable to register receiptor.");
        register("communicator", Tid::new(5))
            .expect("Unable to register communicator.");

        assert!(lookup("communicator").is_some());
        assert!(lookup("rtc").is_some());
        assert!(lookup("send").is_some());

        {
            let option = unregister_tid(&Tid::new(2));

            assert!(option.is_some());
            let names = option.unwrap();

            assert_eq!(names.len(), 2);

            for n in names {
                assert!(lookup(n.as_str()).is_none());
            }
        }

        assert!(lookup("rtc").is_none());
        assert!(lookup("send").is_none());
        assert!(lookup("communicator").is_some());
    }
}