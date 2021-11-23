use core::cmp::Ordering;
use crate::syscalls::c_types;

pub type CTid = c_types::CTid;
pub const NULL_TID: u16 = c_types::NULL_TID;

pub mod bit_array {
    use crate::align::Align;
    use alloc::vec::Vec;

    #[macro_export]
    macro_rules! is_flag_cleared {
        ($flags:expr, $flag:expr) => {
            {
                ($flags) & ($flag) == 0
            }
        }
    }

    #[macro_export]
    macro_rules! is_flag_set {
        ($flags:expr, $flag:expr) => {
            !$crate::is_flag_cleared!($flags, $flag)
        }
    }

    #[macro_export]
    macro_rules! set_flag {
        ($flags:expr, $flag:expr) => {
            {
                $flags |= ($flag)
            }
        }
    }

    #[macro_export]
    macro_rules! clear_flag {
        ($flags:expr, $flag:expr) => {
            {
                $flags &= !($flag)
            }
        }
    }

    #[macro_export]
    macro_rules! is_bit_cleared {
        ($flags:expr, $bit:expr) => {
            $crate::is_flag_cleared!($flags, bit_flag!($bit))
        }
    }

    #[macro_export]
    macro_rules! is_bit_set {
        ($flags:expr, $bit:expr) => {
            !$crate::is_flag_set!($flags, bit_flag!($bit))
        }
    }

    #[macro_export]
    macro_rules! set_bit {
        ($flags:expr, $bit:expr) => {
            $crate::set_flag!($flags, bit_flag!($bit))
        }
    }

    #[macro_export]
    macro_rules! clear_bit {
        ($flags:expr, $bit:expr) => {
            $crate::clear_flag!($flags, bit_flag!($bit))
        }
    }

    #[macro_export]
    macro_rules! bit_flag {
        ($bit:expr) => {
            {
                1 << ($bit)
            }
        }
    }

    type Word = u32;

    pub struct BitArray {
        pub data: Vec<Word>,
        num_bits: usize,
    }

    pub enum BitFilter {
        Zero,
        One,
        Any,
    }

    pub struct BitIterator<'a> {
        bit_array: &'a BitArray,
        index: usize,
        bit_filter: BitFilter,
    }

    impl<'a> BitIterator<'a> {
        pub fn new(bit_array: &'a BitArray, bit_filter: BitFilter) -> Self {
            Self::start_from(0, bit_array, bit_filter)
        }

        pub fn start_from(index: usize, bit_array: &'a BitArray, bit_filter: BitFilter) -> Self {
            BitIterator {
                bit_array,
                index: index.clamp(0, bit_array.num_bits),
                bit_filter,
            }
        }
    }

    impl Iterator for BitIterator<'_> {
        type Item = usize;

        fn next(&mut self) -> Option<Self::Item> {
            if self.index >= self.bit_array.bit_count() {
                None
            } else {
                match self.bit_filter {
                    BitFilter::Any => {
                        let bit = self.index;
                        self.index += 1;

                        Some(bit)
                    },
                    BitFilter::One => {
                        while self.bit_array.bit_count() - self.index >= Word::BITS as usize
                            && self.bit_array.is_word_cleared(self.index) {
                            self.index += Word::BITS as usize - self.index % Word::BITS as usize;
                        }

                        while self.index < self.bit_array.bit_count() && self.bit_array.is_cleared(self.index) {
                            self.index += 1;
                        }

                        if self.index >= self.bit_array.bit_count() {
                            None
                        } else {
                            let option = Some(self.index);
                            self.index += 1;
                            option
                        }
                    },
                    BitFilter::Zero => {
                        while self.bit_array.bit_count() - self.index >= Word::BITS as usize
                            && self.bit_array.is_word_set(self.index) {
                            self.index += Word::BITS as usize - self.index % Word::BITS as usize;
                        }

                        while self.index < self.bit_array.bit_count() && self.bit_array.is_set(self.index) {
                            self.index += 1;
                        }

                        if self.index >= self.bit_array.bit_count() {
                            None
                        } else {
                            let option = Some(self.index);
                            self.index += 1;
                            option
                        }
                    }
                }
            }
        }
    }

    impl BitArray {
        pub const fn bits_per_word() -> usize {
            Word::BITS as usize
        }

        pub const fn word_count(bit_count: usize) -> usize {
            bit_count / Word::BITS as usize +
                if bit_count % Word::BITS as usize != 0 {
                    1
                } else {
                    0
                }
        }

        pub fn new(num_bits: usize, default: bool) -> Self {
            let init_val = if default {
                Word::MAX
            } else {
                0
            };

            Self {
                data: vec![init_val; Self::word_count(num_bits)],
                num_bits,
            }
        }

        pub fn set(&mut self, n: usize) {
            self.set_to(n, true);
        }

        pub fn clear(&mut self, n: usize) {
            self.set_to(n, false);
        }

        pub fn is_cleared(&self, n: usize) -> bool {
            !self.is_set(n)
        }

        pub fn toggle(&mut self, n: usize) {
            self.set_to(n, !self.is_set(n));
        }

        pub fn bit_count(&self) -> usize {
            self.num_bits
        }

        pub fn set_to(&mut self, n: usize, set_true: bool) {
            if n < self.num_bits {
                if set_true {
                    self.data[n / Word::BITS as usize] |= 1 << (n % Word::BITS as usize);
                } else {
                    self.data[n / Word::BITS as usize] &= !(1 << (n % Word::BITS as usize));
                }
            }
        }

        pub fn is_set(&self, n: usize) -> bool {
            n < self.num_bits && self.data[n / Word::BITS as usize] & (1 << (n % Word::BITS as usize)) != 0
        }

        pub fn set_bits_to(&mut self, start_bit: usize, end_bit: usize, set_true: bool) {
            let word_start_bit: usize = start_bit.align(Word::BITS as usize).min(end_bit);
            let word_end_bit: usize = end_bit.align_trunc(Word::BITS as usize).max(start_bit);

            (start_bit..word_start_bit)
                .for_each(|b| self.set_to(b, set_true));

            (word_start_bit..word_end_bit)
                .step_by(Word::BITS as usize)
                .for_each(|b| self.data[b / Word::BITS as usize] = if set_true { Word::MAX } else { 0 });

            (word_end_bit..end_bit)
                .for_each(|b| self.set_to(b, set_true));
        }

        pub fn set_bits(&mut self, start_bit: usize, end_bit: usize) {
            self.set_bits_to(start_bit, end_bit, true);
        }

        pub fn clear_bits(&mut self, start_bit: usize, end_bit: usize) {
            self.set_bits_to(start_bit, end_bit, false);
        }

        pub fn is_word_set(&self, n: usize) -> bool {
            if n < self.num_bits {
                if n / Word::BITS as usize == self.num_bits / Word::BITS as usize {
                    self.data[n / Word::BITS as usize].count_ones() as usize == self.num_bits % Word::BITS as usize
                } else {
                    self.data[n / Word::BITS as usize] == Word::MAX
                }
            } else {
                false
            }
        }

        pub fn is_word_cleared(&self, n: usize) -> bool {
            n < self.num_bits && self.data[n / Word::BITS as usize] == 0
        }

        pub fn first_set(&self) -> Option<usize> {
            let mut index = 0;

            for word in &self.data {
                if word != &0 {
                    return Some(index + word.trailing_zeros() as usize)
                } else {
                    index += Word::BITS as usize;
                }
            }

            None
        }

        pub fn first_cleared(&self) -> Option<usize> {
            let mut index = 0;

            for word in &self.data {
                if word != &Word::MAX {
                    return Some(index + word.trailing_ones() as usize)
                } else {
                    index += Word::BITS as usize;
                }
            }

            None
        }

        pub fn count_zeros(&self) -> usize {
            let sum: usize = self.data
                .iter()
                .take(self.num_bits / Word::BITS as usize)
                .map(|x| x.count_zeros() as usize)
                .sum();

            if self.num_bits % Word::BITS as usize != 0 {
                let bits = !self.data[self.num_bits / Word::BITS as usize] & ((1 << (self.num_bits % Word::BITS as usize)) - 1);
                sum + bits.count_ones() as usize
            } else {
                sum
            }
        }

        pub fn count_ones(&self) -> usize {
            let sum: usize = self.data
                .iter()
                .take(self.num_bits / Word::BITS as usize)
                .map(|x| x.count_ones() as usize)
                .sum();

            if self.num_bits % Word::BITS as usize != 0 {
                let bits = self.data[self.num_bits / Word::BITS as usize] & ((1 << (self.num_bits % Word::BITS as usize)) - 1);
                sum + bits.count_ones() as usize
            } else {
                sum
            }
        }

        pub fn zeros(&self) -> BitIterator<'_> {
            BitIterator::new(self, BitFilter::Zero)
        }

        pub fn ones(&self) -> BitIterator<'_> {
            BitIterator::new(self, BitFilter::One)
        }

        pub fn iter(&self) -> BitIterator<'_> {
            BitIterator::new(self, BitFilter::Any)
        }
    }
}

#[derive(PartialEq, Eq, Copy, Clone, Debug, Hash)]
/// Represents a valid (non-null) thread id a.k.a. tid
pub struct Tid(CTid);

impl Tid {
    /// Create a new `Tid` from a `CTid`. Returns `None` if the `CTid` is `NULL_TID`
    pub fn new(id: CTid) -> Option<Tid> {
        match id {
            NULL_TID => None,
            x => Some(Tid(x)),
        }
    }
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

impl TryFrom<CTid> for Tid {
    type Error = ();

    fn try_from(value: CTid) -> Result<Self, Self::Error> {
        Tid::new(value)
            .ok_or( ())
    }
}

impl TryFrom<&CTid> for Tid {
    type Error = ();

    fn try_from(value: &CTid) -> Result<Self, Self::Error> {
        Tid::try_from(*value)
    }
}

impl From<Tid> for CTid {
    fn from(id: Tid) -> Self {
        id.0
    }
}

impl From<&Tid> for CTid {
    fn from(id: &Tid) -> Self {
        id.0
    }
}

#[cfg(test)]
mod test {
    use super::bit_array::BitArray;

    #[test]
    fn test_bitmap_set() {
        let mut barray: BitArray = BitArray::new(128, false);

        barray.set(2);
        barray.set(5);
        barray.set(24);
        barray.set(107);

        assert!(barray.is_set(2));
        assert!(barray.is_set(5));
        assert!(barray.is_set(24));
        assert!(barray.is_set(107));

        assert!(!barray.is_set(3));
        assert!(!barray.is_set(35));
        assert!(!barray.is_set(62));
        assert!(!barray.is_set(100));
    }

    #[test]
    fn test_bitmap_clear() {
        let mut barray: BitArray = BitArray::new(128, true);

        barray.clear(11);
        barray.clear(58);
        barray.clear(97);
        barray.clear(127);

        assert!(barray.is_cleared(11));
        assert!(barray.is_cleared(58));
        assert!(barray.is_cleared(97));
        assert!(barray.is_cleared(127));

        assert!(!barray.is_cleared(3));
        assert!(!barray.is_cleared(35));
        assert!(!barray.is_cleared(62));
        assert!(!barray.is_cleared(100));
    }

    #[test]
    fn test_bitmap_set_out_of_range() {
        let mut barray: BitArray = BitArray::new(32, false);

        barray.set(38);
        barray.set(107);

        assert!(!barray.is_set(38));
        assert!(!barray.is_set(107));
    }

    #[test]
    fn test_bitmap_clear_out_of_range() {
        let mut barray: BitArray = BitArray::new(32, true);

        barray.clear(38);
        barray.clear(107);

        assert!(barray.is_cleared(38));
        assert!(barray.is_cleared(107));
    }

    #[test]
    fn test_bitmap_first_set() {
        let mut barray: BitArray = BitArray::new(128, false);

        barray.set(10);
        barray.set(18);
        barray.set(39);
        barray.set(77);

        assert_eq!(barray.first_set().unwrap(), 10);
        barray.clear(10);
        assert_eq!(barray.first_set().unwrap(), 18);
        barray.clear(18);
        assert_eq!(barray.first_set().unwrap(), 39);
        barray.clear(39);
        assert_eq!(barray.first_set().unwrap(), 77);
        barray.clear(77);
        assert!(barray.first_set().is_none());
    }

    #[test]
    fn test_bitmap_first_cleared() {
        let mut barray: BitArray = BitArray::new(128, true);

        barray.clear(0);
        barray.clear(16);
        barray.clear(49);
        barray.clear(120);

        assert_eq!(barray.first_cleared().unwrap(), 0);
        barray.set(0);
        assert_eq!(barray.first_cleared().unwrap(), 16);
        barray.set(16);
        assert_eq!(barray.first_cleared().unwrap(), 49);
        barray.set(49);
        assert_eq!(barray.first_cleared().unwrap(), 120);
        barray.set(120);
        assert!(barray.first_cleared().is_none());
    }

    #[test]
    fn test_bitmap_toggle() {
        let mut barray: BitArray = BitArray::new(128, true);

        barray.clear(0);
        barray.clear(16);
        barray.clear(49);
        barray.clear(120);

        barray.toggle(49);
        barray.toggle(0);
        barray.toggle(11);
        barray.toggle(55);

        assert!(barray.is_set(0));
        assert!(barray.is_set(49));
        assert!(barray.is_cleared(55));
        assert!(barray.is_cleared(11));
        assert!(barray.is_cleared(16));
        assert!(barray.is_cleared(120));
        assert!(barray.is_set(100));
        assert!(barray.is_set(7));
    }

    #[test]
    fn test_bitmap_count_bits() {
        let mut barray: BitArray = BitArray::new(32, false);

        barray.set(11);
        barray.set(17);
        barray.set(4);
        barray.set(30);
        barray.set(9);
        barray.set(26);
        barray.set(21);
        barray.set(15);
        barray.set(18);
        barray.set(6);
        barray.set(12);
        barray.set(13);

        assert_eq!(barray.count_ones(), 12);
        assert_eq!(barray.count_zeros(), 20);

        let mut barray2: BitArray = BitArray::new(65, true);

        barray2.clear(4);
        barray2.clear(5);
        barray2.clear(6);
        barray2.clear(17);
        barray2.clear(18);
        barray2.clear(19);
        barray2.clear(30);
        barray2.clear(31);
        barray2.clear(32);
        barray2.clear(64);
        barray2.clear(44);
        barray2.clear(63);

        assert_eq!(barray2.count_ones(), 53);
        assert_eq!(barray2.count_zeros(), 12);
    }
}
