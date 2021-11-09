use alloc::vec::Vec;
use core::mem;
use crate::address::Align;

#[repr(packed)]
pub struct BPB {
    jmp_instr: [u8; 3],
    oem_identifier: [u8; 8],
    bytes_per_sector: u16,
    sectors_per_cluster: u8,
    resd_sectors: u16,
    fat_count: u8,
    root_dir_entry_count: u16,
    sector_count: u16, // if 0, then sector_count >= 65536. actual value is large_sector_count
    media_type: u8,
    sectors_per_fat: u16, // for fat12/fat16 only
    sectors_per_track: u16,
    head_count: u16,
    hidden_sector_count: u32,
    large_sector_count: u32,
}

impl BPB {
    fn fat_type(&self) -> FatFormat {
        let dirent_sectors = (
            (self.root_dir_entry_count as u32 * mem::size_of::<DirectoryEntry>() as u32)
                .align(self.bytes_per_sector as u32)) / self.bytes_per_sector as u32;
        let cluster_count =
            (if self.sector_count == 0 { self.large_sector_count } else { self.sector_count as u32 }
                - self.resd_sectors as u32 - (self.fat_count as u32 * self.sectors_per_fat as u32)
                - dirent_sectors) / self.sectors_per_cluster as u32;

        match cluster_count {
            n if n < 4085 => FatFormat::Fat12,
            n if n < 65525 => FatFormat::Fat16,
            _ => FatFormat::Fat32,
        }
    }
}

#[repr(packed)]
pub struct DirectoryEntry {
    name: [u8; 11],
    attribute: u8,
    _reserved: [u8; 10],
    time: u16,
    date: u16,
    start_cluster: u16,
    size: u32,
}

impl DirectoryEntry {
    pub const READ_ONLY: u8 = 1 << 0;
    pub const HIDDEN: u8 = 1 << 1;
    pub const SYSTEM: u8 = 1 << 2;
    pub const VOL_LABEL: u8 = 1 << 3;
    pub const SUB_DIR: u8 = 1 << 4;
    pub const ARCHIVE: u8 = 1 << 5;

    // Returns true if the char is to be replaced by an underscore

    fn is_replace_char(c: u8) -> bool {
        match c {
            b'+' | b',' | b';' | b'=' | b'[' | b']' => true,
            _ => false
        }
    }

    /* Legal characters are:
       * <space>
       * Numbers: 0-9
       * Upper-case letters: A-Z
       * Punctuation: !, #, $, %, & ', (, ), -, @, ^, _, `, {, }, ~
       * 0x80-0xff (note that 0xE5 is stored as 0x05 on disk)

       0x2E ('.') is reserved for dot entries
    */

    fn is_legal_dos_char(c: u8) -> bool {
        match c {
            b' ' | b'0'..=b'9' | b'A'..=b'Z' | b'!' | b'#' | b'$' | b'%' | b'&' | b'\'' | b'(' | b')'
            | b'-' | b'@' | b'^' | b'_' | b'`' | b'{' | b'}' | b'~' | 0x80..=0xff => true,
            _ => false
        }
    }

    fn short_filename(filename: Vec<u8>) -> Option<([u8; 8], Option<[u8; 3]>, bool)> {
        let mut parts = filename
            .rsplitn(2, |c| *c == b'.');

        if let Some(name) = parts.next() {
            let mut basename = [b' '; 8];
            let mut extension = [b' '; 3];
            let mut oversized = false;

            let base = if let Some(b) = parts.next() {
                // Filename contains extension

                // Replace some invalid chars with underscores, convert lowercase to uppercase, and
                // remove any remaining invalid characters.

                let ext_iter = name
                    .iter()
                    .map(|c| {
                        if c.is_ascii_lowercase() {
                            c.to_ascii_uppercase()
                        } else if Self::is_replace_char(*c) {
                            b'_'
                        } else {
                            *c
                        }
                    })
                    .filter(|c| Self::is_legal_dos_char(*c));

                let mut ext_i = 0;

                for c in ext_iter {
                    // trim any leading spaces
                    if c == b' ' && ext_i == 0 {
                        continue;
                    } else if ext_i == extension.len() {
                        break;
                    }

                    extension[ext_i] = c;
                    ext_i += 1;
                }

                b
            } else {
                // No extension
                name
            };

            // Replace some invalid chars with underscores, convert lowercase to uppercase, and
            // remove any remaining invalid characters.

            let base_iter = base
                .iter()
                .map(|c| {
                    if c.is_ascii_lowercase() {
                        c.to_ascii_uppercase()
                    } else if Self::is_replace_char(*c) {
                        b'_'
                    } else {
                        *c
                    }
                })
                .filter(|c| Self::is_legal_dos_char(*c));

            let mut base_i = 0;

            for c in base_iter {
                // Trim leading spaces
                if c == b' ' && base_i == 0 {
                    continue;
                } else if base_i == basename.len() {
                    // Base name is too long. A tilde and a digit will need to follow the filename,
                    // but the digit depends on how many other similarly named files exist in the
                    // directory.

                    oversized = true;
                    break;
                }

                basename[base_i] = c;
                base_i += 1;
            }

            if extension == [b' ', b' ', b' '] {
                Some((basename, None, oversized))
            } else {
                Some((basename, Some(extension), oversized))
            }
        } else {
            None
        }
    }
}

#[repr(packed)]
pub struct FatBPB {
    bpb: BPB,
    drive_number: u8,
    win_nt_flags: u8,
    signature: u8,      // either 0x28 or 0x29
    volume_id: u32,
    volume_label: [u8; 11],
    system_id: [u8; 8],
    boot_code: [u8; 448],
    partition_signature: u16, // 0xAA55
}

#[repr(packed)]
pub struct Fat32BPB {
    bpb: BPB,
    sectors_per_fat: u32,
    flags: u16,
    fat_version: u16,
    root_dir_cluster: u32,
    fs_info_sector: u16,
    backup_boot_sector: u16,
    _reserved: [u8; 12],
    drive_number: u8,
    win_nt_flags: u8,
    signature: u8,      // either 0x28 or 0x29
    volume_id: u32,
    volume_label: [u8; 11],
    system_id: [u8; 8],
    boot_code: [u8; 420],
    partition_signature: u16, // 0xAA55
}

pub enum ClusterType {
    Free,
    Used,
    Bad,
    Reserved,
    End,
    Invalid,
}

pub enum FatFormat {
    Fat12,
    Fat16,
    Fat32,
}

impl FatFormat {
    pub fn cluster_type(&self, cluster: u32) -> ClusterType {
        match self {
            Self::Fat12 => match cluster {
                0 => ClusterType::Free,
                0xFF8..=0xFFF => ClusterType::End,
                2..=0xFEF => ClusterType::Used,
                0xFF7 => ClusterType::Bad,
                1 | 0xFF0..=0xFF6 => ClusterType::Reserved,
                _ => ClusterType::Invalid,
            },
            Self::Fat16 => match cluster {
                0 => ClusterType::Free,
                0xFFF8..=0xFFFF => ClusterType::End,
                2..=0xFFEF => ClusterType::Used,
                0xFFF7 => ClusterType::Bad,
                1 | 0xFFF0..=0xFFF6 => ClusterType::Reserved,
                _ => ClusterType::Invalid,
            },
            Self::Fat32 => match cluster {
                0 => ClusterType::Free,
                0xFFFFFF8..=0xFFFFFFF => ClusterType::End,
                2..=0xFFFFFEF => ClusterType::Used,
                0xFFFFFF7 => ClusterType::Bad,
                1 | 0xFFFFFF0..=0xFFFFFF6 => ClusterType::Reserved,
                _ => ClusterType::Invalid,
            }
        }
    }
}