use num_traits::Num;
use core::ops::{Index, Not};
use alloc::vec::IntoIter;
use alloc::vec::Vec;
use crate::error;
use core::cmp::{PartialOrd, Ord, Ordering};

#[derive(Copy, PartialEq, Eq, Debug, Hash)]
pub struct MemoryRegion<A>
    where A: Eq + PartialOrd + Ord + Clone + Copy + Num + Not<Output=A> {
    start: A,
    end: A,
}

impl<A> PartialOrd for MemoryRegion<A>
    where A: Eq + PartialOrd + Ord + Clone + Copy + Num + Not<Output=A> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(
            if self.start < other.start {
                Ordering::Less
            } else if self.start > other.start {
                Ordering::Greater
            } else {
                if self.end < other.end {
                    if self.end != MemoryRegion::memory_end() {
                        Ordering::Less
                    } else {
                        Ordering::Greater
                    }
                } else if self.end > other.end {
                    if other.end != MemoryRegion::memory_end() {
                        Ordering::Greater
                    } else {
                        Ordering::Less
                    }
                } else {
                    Ordering::Equal
                }
            }
        )
    }
}

impl<A> Ord for MemoryRegion<A> where A: Eq + PartialOrd + Ord + Clone + Copy + Num + Not<Output=A> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.partial_cmp(other).unwrap()
    }
}

impl<A> MemoryRegion<A>
    where A: Eq + PartialOrd + Ord + Clone + Copy + Num + Not<Output=A> {

    pub fn memory_start() -> A {
        A::zero()
    }

    pub fn memory_end() -> A {
        A::zero()
    }

    /// Create a new `MemoryRegion` with a particular base address and region length.
    /// A region with a start of 0 and a length of 0 spans the entire memory region.

    pub fn new(start: A, end: A) -> MemoryRegion<A> {
        Self {
            start,
            end: if end < start && end != Self::memory_end() { start } else { end },
        }
    }

    /// Creates a new `MemoryRegion` that spans the entire memory region

    pub fn full_region() -> MemoryRegion<A> {
        MemoryRegion::new(Self::memory_start(), Self::memory_end())
    }

    pub fn is_full_region(&self) -> bool {
        self.start == Self::memory_start() && self.end == Self::memory_end()
    }

    pub fn start(&self) -> A {
        self.start
    }

    pub fn end(&self) -> A {
        self.end
    }

    pub fn length(&self) -> Result<A, error::Error> {
        if self.is_full_region() {
            Err(error::LENGTH_OVERFLOW) // overflow
        } else if self.end == MemoryRegion::memory_end() {
            Ok(!self.start + A::one())
        } else {
            Ok(self.end - self.start)
        }
    }

    /// Returns `true` if the other region is either immediately before or after this one.

    pub fn is_adjacent(&self, other: &MemoryRegion<A>) -> bool {
        (self.end == other.start || other.end == self.start) &&
            !self.is_full_region() && !other.is_full_region()
    }

    /// Returns `true` if this region overlaps with the other.

    pub fn overlaps(&self, other: &MemoryRegion<A>) -> bool {
        self.is_full_region() || other.is_full_region()
            || (other.start >= self.start && (other.start < self.end || self.end == Self::memory_end()))
            || (self.start >= other.start && (self.start < other.end || other.end == Self::memory_end()))
    }

    /// Returns `true` if the address is contained within the memory region.

    pub fn contains(&self, address: A) -> bool {
        self.is_full_region() || (address >= self.start && (address < self.end || self.end == Self::memory_end()))
    }

    /// Concatenates two regions into one contiguous region.
    /// Returns the joined `MemoryRegion` on success.
    /// Returns `None` if the two regions are neither overlapping nor adjacent.

    pub fn join(&self, other: &MemoryRegion<A>) -> Option<MemoryRegion<A>> {
        let mut region_set = self.union(other);

        if region_set.disjoint_count() == 1 {
            Some(region_set.regions.remove(0))
        } else {
            None
        }
    }

    /// Takes the set union of two `MemoryRegion`s (i.e. the two regions are concatenated).

    pub fn union(&self, other: &MemoryRegion<A>) -> RegionSet<A> {
        let start = self.start.min(other.start);
        let end = if self.end == Self::memory_end() || other.end == Self::memory_end() {
            Self::memory_end()
        } else {
            self.end.max(other.end)
        };

        if self.is_adjacent(&other) || self.overlaps(&other) {
            RegionSet {
                regions: vec![MemoryRegion::new(start, end)]
            }
        } else {
            RegionSet {
                regions: vec![self.clone(), other.clone()]
            }
        }
    }

    /// Performs the set complement of a `MemoryRegion`

    pub fn complement(&self) -> RegionSet<A> {
        if self.start != Self::memory_start() {
            if self.end != Self::memory_end() {
                RegionSet {
                    regions: vec![MemoryRegion::new(Self::memory_start(), self.start),
                                  MemoryRegion::new(self.end, Self::memory_end())],
                }
            } else {
                RegionSet {
                    regions: vec![MemoryRegion::new( Self::memory_start(), self.start)],
                }
            }
        } else if self.end != Self::memory_end() {
            RegionSet {
                regions: vec![MemoryRegion::new(self.end, Self::memory_end())],
            }
        } else {
            RegionSet::empty()
        }
    }

    /// Takes the set intersection of two `MemoryRegion`s (i.e. the portion that is common to
    /// both regions).

    pub fn intersection(&self, other: &MemoryRegion<A>) -> RegionSet<A> {
        if !self.overlaps(other) {
            // the two regions are disjoint
            RegionSet::empty()
        } else {
            let start = self.start.max(other.start);
            let end = if self.end == Self::memory_end()  {
                other.end
            } else if other.end == Self::memory_end() {
                self.end
            } else {
                self.end.min(other.end)
            };

            RegionSet {
                regions: vec![MemoryRegion::new(start, end)],
            }
        }
    }

    /// Takes the set difference of one `MemoryRegion` from another (i.e. removes the portion in
    /// region B from region A).
    ///
    /// Note: `A - B = A.intersection(!B)`

    pub fn difference(&self, other: &MemoryRegion<A>) -> RegionSet<A> {
        let complement = other.complement();

        match complement.disjoint_count() {
            0 => complement,
            1 => self.intersection(&complement.regions[0]),
            2 => {
                let mut new_region_set = RegionSet::empty();
                let mut diff1 = self.intersection(&complement.regions[0]);
                let mut diff2 = self.intersection(&complement.regions[1]);

                new_region_set.regions.append(&mut diff1.regions);
                new_region_set.regions.append(&mut diff2.regions);

                new_region_set
            },
            n => panic!("Complement of a MemoryRegion shouldn't result in {} MemoryRegions", n),
        }
    }
}

impl<A> Clone for MemoryRegion<A>
    where A: Eq + PartialOrd + Ord + Clone + Copy + Num + Not<Output=A>
{
    fn clone(&self) -> Self {
        MemoryRegion::new(self.start(), self.end())
    }
}


pub struct RegionSet<A>
    where A: Eq + PartialOrd + Ord + Clone + Copy + Num + Not<Output=A>
{
    regions: Vec<MemoryRegion<A>>,
}

impl<A> From<MemoryRegion<A>> for RegionSet<A>
    where A: Eq + PartialOrd + Ord + Clone + Copy + Num + Not<Output=A>
{
    fn from(region: MemoryRegion<A>) -> Self {
        RegionSet {
            regions: vec![region],
        }
    }
}

impl<A> RegionSet<A>
    where A: Eq + PartialOrd + Ord + Clone + Copy + Num + Not<Output=A>
{
    pub fn new(iter: impl Iterator<Item=MemoryRegion<A>>) -> RegionSet<A> {
        let new_region_set = RegionSet::empty();

        iter.fold(new_region_set,
                  |acc, mem_region| acc.union(&RegionSet::from(mem_region))
        )
    }

    pub fn empty() -> RegionSet<A> {
        RegionSet {
            regions: vec![],
        }
    }

    pub fn intersection(&self, other: &RegionSet<A>) -> RegionSet<A> {
        let new_regions = self.regions
            .iter()
            .flat_map(|region|
                other.regions
                    .iter()
                    .filter_map(move |region2| {
                        let mut result = region.intersection(region2);

                        if result.disjoint_count() == 1 {
                            Some(result.regions.remove(0))
                        } else {
                            None
                        }
                    })
            )
            .collect();

        RegionSet {
            regions: new_regions
        }
    }

    pub fn union(&self, other: &RegionSet<A>) -> RegionSet<A> {
        let mut new_regions = self.regions.clone();
        let mut other_regions = other.regions.clone();

        for region in &mut new_regions {
            if other_regions.len() == 0 {
                break;
            } else {
                let (concat_reg, disjoint_reg):
                    (Vec<MemoryRegion<A>>, Vec<MemoryRegion<A>>) =
                    other_regions
                        .iter()
                        .partition(|&other_region| {
                            region.overlaps(other_region)
                                || region.is_adjacent(other_region)
                        });

                *region = concat_reg
                    .iter()
                    .fold(*region,
                          |acc, region2| {
                              acc.join(region2).unwrap()
                          });

                other_regions = disjoint_reg;
            }
        }

        new_regions.append(&mut other_regions);

        RegionSet {
            regions: new_regions
        }
    }

    pub fn complement(&self) -> RegionSet<A> {
        self.regions
            .iter()
            .map(|region| region.complement())
            .fold(RegionSet::from(MemoryRegion::full_region()),
                  |acc, region| acc.intersection(&region))
    }

    pub fn insert(&mut self, new_region: MemoryRegion<A>) {
        let (concat_reg, mut disjoint_reg): (Vec<MemoryRegion<A>>, Vec<MemoryRegion<A>>) =
            self.regions
                .iter()
                .partition(|&region| {
                    new_region.overlaps(region) || new_region.is_adjacent(region)
                });

        disjoint_reg.push(concat_reg
            .iter()
            .fold(new_region,
                  |acc, region2| acc.join(region2).unwrap()));

        self.regions = disjoint_reg;
    }

    pub fn disjoint_count(&self) -> usize {
        self.regions.len()
    }

    pub fn iter(&self) -> core::slice::Iter<'_, MemoryRegion<A>> {
        self.regions.iter()
    }

    pub fn iter_mut(&mut self) -> core::slice::IterMut<'_, MemoryRegion<A>> {
        self.regions.iter_mut()
    }

    pub fn into_iter(self) -> IntoIter<MemoryRegion<A>> {
        self.regions.into_iter()
    }
}

impl<A> Index<usize> for RegionSet<A>
    where A: Eq + PartialOrd + Ord + Clone + Copy + Num + Not<Output=A>,
{
    type Output = MemoryRegion<A>;

    fn index(&self, index: usize) -> &Self::Output {
        &self.regions[index]
    }
}

#[cfg(test)]
mod test {
    use super::{MemoryRegion, RegionSet};
    use crate::address::{self, VAddr, u32_into_vaddr};
    use crate::error;

    #[test]
    fn test_memory_region() {
        let region1: MemoryRegion<u64> = MemoryRegion::new(MemoryRegion::<u64>::memory_start(),
                                                           0xA0000u64);
        let region2: MemoryRegion<u64> = MemoryRegion::new(MemoryRegion::<u64>::memory_start(),
                                                           MemoryRegion::<u64>::memory_end());
        let region3: MemoryRegion<u64> = MemoryRegion::new(0x100000,
                                                           0x300000);
        let region4: MemoryRegion<u64> = MemoryRegion::new(0x80000000,
                                                           MemoryRegion::<u64>::memory_end());

        assert_eq!(region1.start, MemoryRegion::<u64>::memory_start());
        assert_eq!(region1.end, 0xA0000u64);

        assert_eq!(region2.start, MemoryRegion::<u64>::memory_start());
        assert_eq!(region2.end, MemoryRegion::<u64>::memory_end());

        assert_eq!(region3.start, 0x100000);
        assert_eq!(region3.end, 0x300000);

        assert_eq!(region4.start, 0x80000000);
        assert_eq!(region4.end, MemoryRegion::<u64>::memory_end());
    }

    #[test]
    fn test_join_region() {
        let region1 = MemoryRegion::<VAddr, usize>::new(address::u32_into_vaddr(0x1000), address::u32_into_vaddr(0x2000));
        let region2 = MemoryRegion::<VAddr, usize>::new(address::u32_into_vaddr(0x2000), address::u32_into_vaddr(0x3000));
        let region3 = MemoryRegion::<VAddr, usize>::new(address::u32_into_vaddr(0x1000), address::u32_into_vaddr(0x3000));
        let region4 = MemoryRegion::<VAddr, usize>::new(address::u32_into_vaddr(0x1800), address::u32_into_vaddr(0x3000));
        let region5 = MemoryRegion::<VAddr, usize>::new(address::u32_into_vaddr(0x1000), address::u32_into_vaddr(0x4000));

        assert_ne!(&region1, &region2);
        assert_ne!(&region2, &region3);
        assert_ne!(&region1, &region3);

        let joined_region1 = region1.join(&region2);
        let joined_region2 = region1.join(&region4);

        assert!(joined_region1.is_ok());
        assert!(joined_region2.is_ok());

        let joined_region1 = joined_region1.unwrap();
        let joined_region2 = joined_region2.unwrap();

        assert_eq!(&joined_region1, &region3);
        assert_eq!(&joined_region2, &region3);
        assert_ne!(&joined_region2, &region5);
    }

    #[test]
    fn test_memory_region_length() {
        let region1: MemoryRegion<u64> = MemoryRegion::new(MemoryRegion::<u64>::memory_start(),
                                                           0xA0000u64);
        let region2: MemoryRegion<u64> = MemoryRegion::new(MemoryRegion::<u64>::memory_start(),
                                                           MemoryRegion::<u64>::memory_end());
        let region3: MemoryRegion<u64> = MemoryRegion::new(0x100000,
                                                           0x300000);
        let region4: MemoryRegion<u64> = MemoryRegion::new(0x80000000,
                                                           MemoryRegion::<u64>::memory_end());

        assert_eq!(region1.length(), Ok(0xA0000u64));
        assert_eq!(region2.length(), Err(error::LENGTH_OVERFLOW));
        assert_eq!(region3.length(), Ok(0x200000));
        assert_eq!(region4.length(), Ok(0xFFFFFFFF_80000000));
    }

    #[test]
    fn test_memory_region_complement() {
        let region1: MemoryRegion<u64> = MemoryRegion::new(MemoryRegion::<u64>::memory_start(),
                                                           0xA0000u64);

        let region_set = region1.complement();

        assert_eq!(region_set.regions.len(), 1);
        assert_eq!(region_set.regions[0].start, 0xA0000u64);
        assert_eq!(region_set.regions[0].end, MemoryRegion::<u64>::memory_end());
    }

    #[test]
    fn test_region_set() {
        let region1: MemoryRegion<u64> = MemoryRegion::new(0x100000, 0x300000);
        let region2: MemoryRegion<u64> = MemoryRegion::new(0x200000, 0x500000);
        let region3: MemoryRegion<u64> = MemoryRegion::new(0x1000000, 0x1001000);

        let region_set1: RegionSet<u64> = RegionSet::new(vec![region1, region2, region3].into_iter());

        assert_eq!(region_set1.disjoint_count(), 2);
        assert_eq!(&region_set1[0], &MemoryRegion::new(0x100000, 0x500000));
        assert_eq!(&region_set1[1], &MemoryRegion::new(0x1000000, 0x1001000));
    }

    #[test]
    fn test_region_set_intersection() {
        let region_set1: RegionSet<u64> = RegionSet::new(vec![MemoryRegion::new(0x100000, 0x300000), MemoryRegion::new(0xA00000, 0xB00000)].into_iter());
        let region_set2 = RegionSet::from(MemoryRegion::new(0x200000u64, 0x500000u64));

        let region_set3 = region_set1.intersection(&region_set2).intersection(&RegionSet::from(MemoryRegion::full_region()));

        assert_eq!(region_set3.disjoint_count(), 1);
        assert_eq!(&region_set3[0], &MemoryRegion::new(0x200000, 0x300000));
    }
}