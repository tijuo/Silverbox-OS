use crate::region::{MemoryRegion, RegionSet};
use crate::address::{PAddr, PSize};
use crate::multiboot::{RawMemoryMap, RawMemoryMapIterator};

use rust::types::bit_array::{BitArray, BitIterator, BitFilter};
use rust::align::Align;
use alloc::vec::Vec;
use crate::page::PhysicalFrame;

static mut PAGE_ALLOCATOR: Option<PhysPageAllocator> = None;
static mut BOOTSTRAP_MEM: Option<BootstrapAllocator> = None;

/// 1 + the highest physical address that can be accessed with PSE
pub const MAX_PHYS_ADDR: PAddr = 1 << 40;

/// 1 + the highest physical address that can be accessed via 4 kiB pages
const MAX_PHYS_ADDR_4K: PAddr = 1 << 32;

struct BootstrapAllocator {
    start: PAddr,
    end: PAddr,
}

impl BootstrapAllocator {
    /*
    pub fn is_available(&self, block_size: BlockSize) -> bool {
        lowlevel::print_debugln("Checking if available...");

        let val = self.mmap_iter
            .clone()
            .any(|mmap| {
                let new_end = self.end + block_size.bytes();
                let mmap_end = mmap.base_addr + mmap.length;

                lowlevel::print_debug("Mmap start: 0x");
                lowlevel::print_debug_u32(mmap.base_addr as u32, 16);
                lowlevel::print_debug("Mmap end: 0x");
                lowlevel::print_debug_u32(mmap_end as u32, 16);
                lowlevel::print_debug("End: 0x");
                lowlevel::print_debug_u32(self.end as u32, 16);
                lowlevel::print_debug("New end: 0x");
                lowlevel::print_debug_u32(new_end as u32, 16);
                lowlevel::print_debug_u32(mmap.map_type as u32, 10);
                lowlevel::print_debugln("");

                mmap.map_type == RawMemoryMap::AVAILABLE && self.end >= mmap.base_addr
                    && self.end < mmap_end && new_end >= mmap.base_addr && new_end < mmap_end
            });

        lowlevel::print_debug("Unable to allocate block of size ");
        lowlevel::print_debug_u32(block_size.bytes() as u32, 10);
        lowlevel::print_debug(" at address 0x");
        lowlevel::print_debug_u32(self.end as u32, 16);
        lowlevel::print_debugln("");

        val
    }
*/

    pub fn extend(&mut self, block_size: BlockSize) -> Result<(PAddr, BlockSize), AllocError> {
        let new_addr = self.end;

        self.end += block_size.bytes() as PSize;

        Ok((new_addr, block_size))
    }
}

#[derive(Debug, Copy, Clone)]
pub enum AllocError {
    TooBig,
    OutOfMemory,
}

#[derive(Copy, Clone)]
pub enum BlockSize {
    Block4k,
    Block128k,
    Block4M,
    Block128M,
}

impl BlockSize {
    pub fn new(level: usize) -> BlockSize {
        match level {
            0 => BlockSize::Block4k,
            1 => BlockSize::Block128k,
            2 => BlockSize::Block4M,
            3 => BlockSize::Block128M,
            l => panic!("Level {} block doesn't exist.", l),
        }
    }

    pub const fn total_sizes() -> usize { 4 }

    pub const fn level(&self) -> usize {
        match self {
            BlockSize::Block4k => 0,
            BlockSize::Block128k => 1,
            BlockSize::Block4M => 2,
            BlockSize::Block128M => 3,
        }
    }

    pub const fn bytes(&self) -> PSize {
        match self {
            BlockSize::Block4k => 4*1024,
            BlockSize::Block128k => 128*1024,
            BlockSize::Block4M => 4*1024*1024,
            BlockSize::Block128M => 128*1024*1024,
        }
    }
}

pub struct PhysPageAllocator {
    memory_size: PSize,

    // Each BitArray corresponds to the free/occupied status a BlockSize level
    // If bit is 0, then the block is free.
    // If bit is 1, then block contains at least one used sub-block
    occupied_status: Vec<BitArray>,

    // Each BitArray corresponds to the available/filled status a BlockSize level
    // If bit is 0, then the block contains at least one free sub-block.
    // If bit is 1, then the block is completely used and doesn't contain any free sub-blocks
    filled_status: Vec<BitArray>,

    // Free/occupied status of a block above 4G (pages must be 4M)
    occupied_status_upper: Vec<BitArray>,

    // Available/filled status of a block above 4G (pages must be 4M)
    filled_status_upper: Vec<BitArray>,

    // The regions that cannot be allocated (because they're: being used by the kernel, MMIO ranges,
    // non-existent, marked as bad, etc.)
    resd_regions: RegionSet<PAddr>,
}

pub fn allocator() -> &'static PhysPageAllocator {
    unsafe {
        PAGE_ALLOCATOR
            .as_ref()
            .unwrap_or_else(|| panic!("Physical page allocator hasn't been initialized yet."))
    }
}

pub fn allocator_mut() -> &'static mut PhysPageAllocator {
    unsafe {
        PAGE_ALLOCATOR
            .as_mut()
            .unwrap_or_else(|| panic!("Physical page allocator hasn't been initialized yet."))
    }
}

pub fn is_bootstrap_ready() -> bool {
    unsafe {
        BOOTSTRAP_MEM.is_some()
    }
}

pub fn is_allocator_ready() -> bool {
    unsafe {
        PAGE_ALLOCATOR.is_some()
    }
}

pub fn alloc_phys(block_size: BlockSize) -> Result<(PAddr, BlockSize), AllocError> {
    if is_allocator_ready() {
        allocator_mut().alloc(block_size)
    } else if is_bootstrap_ready() {
        let bootstrap_alloc = unsafe {
            BOOTSTRAP_MEM.as_mut()
                .unwrap_or_else(|| panic!("Unable to retrieve bootstrap allocator"))
        };

        bootstrap_alloc.extend(block_size)
    } else {
        panic!("Bootstrap allocator hasn't been initialized yet.");
    }
}

pub fn release_phys(address: PAddr, block_size: BlockSize) {
    if is_allocator_ready() {
        allocator_mut().release(address, block_size)
    } /*else if !is_bootstrap_ready() {
        panic!("Bootstrap allocator hasn't been initialized yet.");
    }*/
}

impl PhysPageAllocator {
    pub fn init_bootstrap(first_free_page: PAddr) {
        if is_allocator_ready() {
            panic!("Physical memory allocator has already been initialized.");
        } else if is_bootstrap_ready() {
            panic!("Bootstrap allocator has already been initialized.");
        } else {
            unsafe {
                BOOTSTRAP_MEM.replace(BootstrapAllocator {
                    start: first_free_page,
                    end: first_free_page,
                });
            }
        }
    }

    pub fn init(mmap_iter: RawMemoryMapIterator) {
        if is_allocator_ready() {
            panic!("Physical page allocator has already been initialized.");
        }

        let mut memory_end: PSize = 0;

        mmap_iter
            .clone()
            .for_each(|mmap| {
                if mmap.map_type == RawMemoryMap::AVAILABLE {
                    memory_end = memory_end.max(mmap.base_addr + mmap.length);
                }
        });

        memory_end = if memory_end == 0 {
            MAX_PHYS_ADDR
        } else {
            memory_end.align(PhysicalFrame::SMALL_PAGE_SIZE as PSize)
                .clamp(1, MAX_PHYS_ADDR)
        };

        let mut allocator = PhysPageAllocator {
            memory_size: memory_end,
            occupied_status: Vec::new(),
            filled_status: Vec::new(),
            occupied_status_upper: Vec::new(),
            filled_status_upper: Vec::new(),
            resd_regions: RegionSet::empty(),
        };

        // Create the block status bit arrays for the first 4G

        for i in 0..BlockSize::total_sizes() {
            let block_size = BlockSize::new(i);
            let blocks = memory_end.clamp(0, MAX_PHYS_ADDR_4K).align(block_size.bytes()) / block_size.bytes();

            allocator.occupied_status.push(BitArray::new(blocks as usize, false));

            // 4k blocks have no sub blocks, so don't add status bits for them

            if i > 0 {
                allocator.filled_status.push(BitArray::new(blocks as usize, false));
            }
        }

        if memory_end > MAX_PHYS_ADDR_4K {
            // Create the block status bit arrays

            for i in BlockSize::Block4M.level()..BlockSize::total_sizes() {
                let block_size = BlockSize::new(i);
                let blocks = (memory_end.clamp(0, MAX_PHYS_ADDR) - MAX_PHYS_ADDR_4K)
                    .align(block_size.bytes()) / block_size.bytes();

                allocator.occupied_status_upper.push(BitArray::new(blocks as usize, false));

                // 4k blocks have no sub blocks, so don't add status bits for them

                if i > BlockSize::Block4M.level() {
                    allocator.filled_status_upper.push(BitArray::new(blocks as usize, false));
                }
            }
        }

        let bootstrap_alloc = unsafe { BOOTSTRAP_MEM.as_mut()
            .unwrap_or_else(|| panic!("Unable to get bootstrap allocator")) };

        // Mark any reserved pages as used

        for mmap in mmap_iter.clone() {
            if mmap.map_type == RawMemoryMap::AVAILABLE && mmap.base_addr >= bootstrap_alloc.end {
                continue;
            }

            let end = if mmap.base_addr + mmap.length > bootstrap_alloc.end
                && mmap.map_type == RawMemoryMap::AVAILABLE {
                bootstrap_alloc.end
            } else {
                mmap.base_addr + mmap.length
            };

            if mmap.base_addr < memory_end {
                let mmap_start = mmap.base_addr;
                let block128k_start = mmap_start.align(BlockSize::Block128k.bytes());
                let mmap_end = end.clamp(0, memory_end);
                let block128k_end = mmap_end.align_trunc(BlockSize::Block128k.bytes());

                for address in (mmap.base_addr..block128k_start)
                    .step_by(BlockSize::Block4k.bytes() as usize) {
                    allocator.mark_used(address, BlockSize::Block4k);
                }

                for address in (block128k_start..block128k_end)
                    .step_by(BlockSize::Block128k.bytes() as usize) {
                    allocator.mark_used(address, BlockSize::Block128k);
                }

                for address in (block128k_end..mmap_end)
                    .step_by(BlockSize::Block4k.bytes() as usize) {
                    allocator.mark_used(address, BlockSize::Block4k);
                }
            }

            match mmap.map_type {
                RawMemoryMap::AVAILABLE if end > bootstrap_alloc.end => (),
                _ => allocator.mark_reserved(MemoryRegion::new(mmap.base_addr, end)),
            };
        }

        // Mark the bootstrap pages as used
        // todo(): This doesn't free any unused bootstrap pages

        (bootstrap_alloc.start..bootstrap_alloc.end)
            .step_by(PhysicalFrame::SMALL_PAGE_SIZE as usize)
            .for_each(|addr| allocator.mark_used(addr, BlockSize::Block4k));

        // From this point on, we'll use the physical memory allocator instead of the bootstrap
        // allocator for physical pages

        unsafe {
            PAGE_ALLOCATOR.replace(allocator);
            BOOTSTRAP_MEM.take();
        }

        // Any holes in the memory map should also be marked as reserved

        let hole_regions = mmap_iter
            .clone()
            .map(|mmap| RegionSet::from(MemoryRegion::new(mmap.base_addr, mmap.base_addr + mmap.length)) as RegionSet<PAddr>)
            .fold(RegionSet::from(MemoryRegion::new(0, memory_end)),
                  |set_result, region| region.complement().intersection(&set_result));

        for r in hole_regions.into_iter() {
            let alloc_mut = allocator_mut();

            let mmap_start = r.start();
            let block128k_start = mmap_start.align(BlockSize::Block128k.bytes());
            let mmap_end = r.end().clamp(0, memory_end);
            let block128k_end = r.end().align_trunc(BlockSize::Block128k.bytes());

            for address in (mmap_start..block128k_start)
                .step_by(BlockSize::Block4k.bytes() as usize) {
                alloc_mut.mark_used(address, BlockSize::Block4k);
            }

            for address in (block128k_start..block128k_end)
                .step_by(BlockSize::Block128k.bytes() as usize) {
                alloc_mut.mark_used(address, BlockSize::Block128k);
            }

            for address in (block128k_end..mmap_end)
                .step_by(BlockSize::Block4k.bytes() as usize) {
                alloc_mut.mark_used(address, BlockSize::Block4k);
            }

            allocator_mut().mark_reserved(r);
        }
    }

    fn filled_status(&self, block: BlockSize) -> &BitArray {
        match block {
            BlockSize::Block4k => &self.occupied_status[0],
            b => &self.filled_status[b.level()-1],
        }
    }

    fn filled_status_mut(&mut self, block: BlockSize) -> &mut BitArray {
        match block {
            BlockSize::Block4k => &mut self.occupied_status[0],
            b => &mut self.filled_status[b.level()-1],
        }
    }

    fn filled_status_upper(&self, block: BlockSize) -> &BitArray {
        match block {
            BlockSize::Block4M => &self.occupied_status_upper[0],
            b if b.level() < BlockSize::Block4M.level() => panic!("Block level {} is not supported.", b.level()),
            b => &self.filled_status_upper[b.level()-BlockSize::Block4M.level()-1],
        }
    }

    fn filled_status_upper_mut(&mut self, block: BlockSize) -> &mut BitArray {
        match block {
            BlockSize::Block4M => &mut self.occupied_status_upper[0],
            b if b.level() < BlockSize::Block4M.level() => panic!("Block level {} is not supported.", b.level()),
            b => &mut self.filled_status_upper[b.level()-BlockSize::Block4M.level()-1],
        }
    }

    pub fn is_block_free(&self, address: PAddr, block_size: BlockSize) -> bool {
        if address >= MAX_PHYS_ADDR_4K {
            if block_size.level() < BlockSize::Block4M.level() {
                panic!("Block of size {} doesn't exist for {:#x}", block_size.bytes(), address);
            } else {
                self.occupied_status_upper[block_size.level() - BlockSize::Block4M.level()]
                    .is_cleared(((address - MAX_PHYS_ADDR_4K) / block_size.bytes()) as usize)
            }
        } else {
            self.occupied_status[block_size.level()].is_cleared((address / block_size.bytes()) as usize)
        }
    }

    pub fn is_block_partially_used(&self, address: PAddr, size: BlockSize) -> bool {
        if address >= MAX_PHYS_ADDR_4K {
            if size.level() < BlockSize::Block4M.level() {
                panic!("Block of size {} doesn't exist for {:#x}", size.bytes(), address);
            } else {
                let bit_index = ((address - MAX_PHYS_ADDR_4K) / size.bytes()) as usize;

                self.occupied_status_upper[size.level() - BlockSize::Block4M.level()].is_set(bit_index)
                    && self.filled_status_upper(size).is_cleared(bit_index)
            }
        } else {
            let bit_index = (address / size.bytes()) as usize;

            self.occupied_status[size.level()].is_set(bit_index)
                && self.filled_status(size).is_cleared(bit_index)
        }
    }

    pub fn is_block_used(&self, address: PAddr, size: BlockSize) -> bool {
        if address >= MAX_PHYS_ADDR_4K {
            if size.level() < BlockSize::Block4M.level() {
                panic!("Block of size {} doesn't exist for {:#x}", size.bytes(), address);
            } else {
                self.filled_status_upper(size).is_set(((address - MAX_PHYS_ADDR_4K) / size.bytes()) as usize)
            }
        } else {
            self.filled_status(size).is_set((address / size.bytes()) as usize)
        }
    }

    /// Mark a block (and possibly its super-block(s)) as used.

    fn mark_used(&mut self, address: PAddr, size: BlockSize) {
        if address >= MAX_PHYS_ADDR_4K && size.level() < BlockSize::Block4M.level() {
            panic!("Cannot mark block of size {} @ {:#x} as used.", size.bytes(), address);
        }

        // Mark this block and all of its sub-blocks as used
        {
            let mut bit_start = if address >= MAX_PHYS_ADDR_4K {
                ((address - MAX_PHYS_ADDR_4K) / size.bytes()) as usize
            } else {
                (address / size.bytes()) as usize
            };

            let mut bit_end = bit_start + 1;

            for level in (0..size.level() + 1).rev() {
                if address >= MAX_PHYS_ADDR_4K {
                    if level < BlockSize::Block4M.level() {
                        continue;
                    }

                    self.occupied_status_upper[level - BlockSize::Block4M.level()]
                        .set_bits(bit_start, bit_end);

                    // Only blocks larger than 4 MB have sub-blocks to be marked as used

                    if level > BlockSize::Block4M.level() {
                        self.filled_status_upper_mut(BlockSize::new(level))
                            .set_bits(bit_start, bit_end);
                    }
                } else {
                    self.occupied_status[level].set_bits(bit_start, bit_end);

                    // Only blocks larger than 4 kB have sub-blocks to be marked as used

                    if level > 0 {
                        self.filled_status_mut(BlockSize::new(level))
                            .set_bits(bit_start, bit_end);
                    }
                }

                bit_start *= BitArray::bits_per_word();
                bit_end *= BitArray::bits_per_word();
            }
        }

        /* Mark the super-blocks as partially used, unless the super-blocks' sub-blocks are
           all marked as used (in that case, mark the super-block as completely used.) */

        for level in size.level()..BlockSize::total_sizes() {
            let level_size = BlockSize::new(level).bytes();

            if address >= MAX_PHYS_ADDR_4K {
                let bit_index = ((address - MAX_PHYS_ADDR_4K) / level_size) as usize;

                if level > size.level() {
                    self.occupied_status_upper[level - BlockSize::Block4M.level()].set(bit_index);
                }

                if self.filled_status_upper(BlockSize::new(level)).is_word_set(bit_index)
                    && level + 1 < BlockSize::total_sizes() {
                    self.filled_status_upper_mut(BlockSize::new(level + 1))
                        .set(((address - MAX_PHYS_ADDR_4K) / BlockSize::new(level + 1).bytes()) as usize);
                }
            } else {
                let bit_index = (address / level_size) as usize;

                if level > size.level() {
                    self.occupied_status[level].set(bit_index);
                }

                if self.filled_status(BlockSize::new(level)).is_word_set(bit_index)
                    && level + 1 < BlockSize::total_sizes() {
                    self.filled_status_mut(BlockSize::new(level + 1))
                        .set((address / BlockSize::new(level + 1).bytes()) as usize);
                }
            }
        }
    }

    /// Mark a block as free.

    fn mark_free(&mut self, address: PAddr, size: BlockSize) {
        let mut bit_start = if address >= MAX_PHYS_ADDR_4K {
            ((address - MAX_PHYS_ADDR_4K) / size.bytes()) as usize
        } else {
            (address / size.bytes()) as usize
        };

        let mut bit_end = bit_start+1;

        for level in (0..size.level()+1).rev() {
            if address >= MAX_PHYS_ADDR_4K {
                if level < BlockSize::Block4M.level() {
                    continue;
                }

                self.occupied_status_upper[level - BlockSize::Block4M.level()].clear_bits(bit_start, bit_end);

                if level > BlockSize::Block4M.level() {
                    self.filled_status_upper_mut(BlockSize::new(level)).clear_bits(bit_start, bit_end);
                }
            } else {
                self.occupied_status[level].clear_bits(bit_start, bit_end);

                if level > 0 {
                    self.filled_status_mut(BlockSize::new(level)).clear_bits(bit_start, bit_end);
                }
            }

            bit_start *= BitArray::bits_per_word();
            bit_end *= BitArray::bits_per_word();
        }
    }

    fn mark_reserved(&mut self, region: MemoryRegion<PAddr>) {
        self.resd_regions.insert(region);
    }

    pub fn alloc(&mut self, size: BlockSize) -> Result<(PAddr, BlockSize), AllocError> {
        let result = self
            .find_block(size)
            .map(|addr| (addr, size))
            .ok_or_else(|| {
                let filled_status = if size.level() >= BlockSize::Block4M.level() {
                    allocator()
                        .filled_status_upper(BlockSize::new(BlockSize::total_sizes() - 1))
                } else {
                    allocator()
                        .filled_status(BlockSize::new(BlockSize::total_sizes() - 1))
                };

                if filled_status.count_ones() == filled_status.bit_count() {
                    AllocError::OutOfMemory
                } else {
                    AllocError::TooBig
                }
            })?;

        self.mark_used(result.0, result.1);

        Ok(result)
    }

    fn find_block(&self, size: BlockSize) -> Option<PAddr> {
        self._find_block(0, BlockSize::total_sizes()-1, size)
    }

    // todo: Implement _find_block for 4M blocks and up in the upper 4G section

    fn _find_block(&self, start_index: usize, level: usize, size: BlockSize) -> Option<PAddr> {
        if level == size.level() {
            BitIterator::start_from(start_index, &self.occupied_status[level], BitFilter::Zero)
                .next()
                .map(|b| (b as PSize * size.bytes()) as PAddr)
        } else {
            for b in BitIterator::start_from(start_index, &self.filled_status(BlockSize::new(level)), BitFilter::Zero) {
                if let option@Some(_) = self._find_block(BitArray::bits_per_word()*start_index + b, level - 1, size) {
                    return option;
                }
            }

            None
        }
    }

    pub fn is_free(&self, address: PAddr) -> bool {
        self.is_block_free(address, BlockSize::Block4k)
    }

    pub fn is_used(&self, address: PAddr) -> bool {
        self.is_block_used(address, BlockSize::Block4k)
    }

    pub fn is_reserved(&self, address: PAddr) -> bool {
        allocator()
            .resd_regions
            .iter()
            .any(|region| region.contains(address))
    }

    pub fn contains(&self, address: PAddr) -> bool {
        address < self.memory_size
            && !self.is_reserved(address)
    }

    pub fn release(&mut self, address: PAddr, block_size: BlockSize) {
        if self.is_block_free(address, block_size) {
            panic!("Attempted to release a {}-byte block at {:#x} that's already free", block_size.bytes(), address);
        } else {
            self.mark_free(address, block_size);
        }
    }

    pub fn free_count(&self, size: BlockSize) -> usize {
        self.occupied_status[size.level()].count_zeros() + if size.level() >= BlockSize::Block4M.level() {
            self.occupied_status_upper[size.level() - size.level()].count_zeros()
        } else {
            0
        }
    }

    pub fn used_count(&self, size: BlockSize) -> usize {
        self.filled_status(size).count_ones() + if size.level() >= BlockSize::Block4M.level() {
            self.filled_status_upper(size).count_ones()
        } else {
            0
        }
    }

    pub fn total_count(&self, size: BlockSize) -> usize {
        self.occupied_status[size.level()].bit_count() + if size.level() >= BlockSize::Block4M.level() {
            self.occupied_status_upper[size.level() - size.level()].bit_count()
        } else {
            0
        }
    }
}

/*
#[cfg(test)]
mod test {
    use crate::page::PhysicalFrame;
    use super::new_allocator::Page;
    use rust::align::Align;
    use super::BlockSize;
    use crate::address::{PSize, PAddr};
    use alloc::vec::Vec;

    #[test]
    fn test_allocation() {
        let (start_addr, total_mem) = (0x1000, 0x10000);
        super::allocator().init_test(start_addr, total_mem);

        assert_eq!(super::allocator().total_count(BlockSize::Block4k), (total_mem-start_addr) / PhysicalFrame::SMALL_PAGE_SIZE);
        assert_eq!(super::allocator().total_count(BlockSize::Block4k), super::allocator().free_count(BlockSize::Block4k));
        assert_eq!(super::allocator().used_count(BlockSize::Block4k), 0);

        let alloc_result = super::allocator().alloc(BlockSize::Block4k);

        assert!(alloc_result.is_some());

        let address = alloc_result.unwrap().0;

        assert!(address >= start_addr);
        assert!(address < total_mem as PAddr);
        assert!(address.is_aligned(PhysicalFrame::SMALL_PAGE_SIZE as PSize));
        assert!(!super::allocator().is_free(address));
        assert!(super::allocator().is_used(address));

        let page2 = page.clone();

        super::allocator().release(address, BlockSize::Block4k);

        assert!(super::allocator().is_free(address));
        assert!(super::allocator().is_used(address));
    }

    #[test]
    fn test_free_page_count() {
        let (start_addr, total_mem) = (0x0000, 0x100000);
        super::allocator().init_test(start_addr, total_mem);

        assert_eq!(super::allocator().free_count(BlockSize::Block4k), (total_mem-start_addr as PSize) / PhysicalFrame::SMALL_PAGE_SIZE);

        let p1 = super::allocator().alloc(BlockSize::Block4k).unwrap().0;
        let p2 = super::allocator_mut().alloc(BlockSize::Block4k).unwrap().0;
        let p3 = super::allocator_mut().alloc(BlockSize::Block4k).unwrap().0;

        assert_eq!(super::allocator().free_count(BlockSize::Block4k), ((total_mem-start_addr as PSize) / PhysicalFrame::SMALL_PAGE_SIZE - 3) as PSize);

        [p1, p2, p3]
            .iter()
            .for_each(|a| super::allocator_mut().release(*a, BlockSize::Block4k));

        assert_eq!(super::allocator().free_count(BlockSize::Block4k), (total_mem-start_addr as PSize) / PhysicalFrame::SMALL_PAGE_SIZE);
    }

    #[test]
    fn test_used_page_count() {
        let (start_addr, total_mem) = (0x0000, 0x100000);
        super::allocator().init_test(start_addr, total_mem);

        assert_eq!(super::allocator().used_count(BlockSize::Block4k), 0);

        let p1 = super::allocator_mut().alloc(BlockSize::Block4k).unwrap().0;
        let p2 = super::allocator_mut().alloc(BlockSize::Block4k).unwrap().0;
        let p3 = super::allocator_mut().alloc(BlockSize::Block4k).unwrap().0;

        assert_eq!(super::allocator().used_count(BlockSize::Block4k), 3);

        [p1, p2, p3]
            .iter()
            .for_each(|a| super::allocator_mut().release(*a, BlockSize::Block4k));

        assert_eq!(super::allocator().used_count(BlockSize::Block4k), 0);
    }

    #[test]
    fn test_empty_allocator() {
        let (start_addr, total_mem) = (0x0000, 0x0000);

        super::allocator().init_test(start_addr, total_mem);
        assert_eq!(super::allocator().free_count(BlockSize::Block4k), 0);
        assert!(super::allocator_mut().alloc(BlockSize::Block4k).is_none());
    }

    #[test]
    #[should_panic]
    fn test_double_release() {
        let (start_addr, total_mem) = (0x0000, 0x10000);
        super::allocator().init_test(start_addr, total_mem);

        let result = super::allocator_mut().alloc(BlockSize::Block4k);

        assert!(result.is_some());

        let page = result.unwrap().0;
        let cloned_page = page.clone().0;

        super::allocator().release(page, BlockSize::Block4k);
        super::allocator().release(cloned_page, BlockSize::Block4k);
    }

    #[test]
    fn test_page_sizes() {
        assert!(PhysicalFrame::SMALL_PAGE_SIZE < PhysicalFrame::PAE_LARGE_PAGE_SIZE);
        assert!(PhysicalFrame::PAE_LARGE_PAGE_SIZE < PhysicalFrame::HUGE_PAGE_SIZE);
        assert!(PhysicalFrame::SMALL_PAGE_SIZE > 0);
        assert_eq!(PhysicalFrame::PAE_LARGE_PAGE_SIZE % PhysicalFrame::SMALL_PAGE_SIZE, 0);
        assert_eq!(PhysicalFrame::HUGE_PAGE_SIZE % PhysicalFrame::SMALL_PAGE_SIZE, 0);
        assert_eq!(PhysicalFrame::HUGE_PAGE_SIZE % PhysicalFrame::PAE_LARGE_PAGE_SIZE, 0);
    }
}
 */