#ifndef ATA_H
#define ATA_H

#define ATA_PORT		0x1F0

// Port offsets

#define ATA_DATA		0
#define ATA_FEATURES		1
#define ATA_ERROR		1
#define ATA_SEC_CNT		2
#define ATA_SEC_NUM		3
#define ATA_LBA_LO		3
#define ATA_CYL_LO		4
#define ATA_LBA_MID		4
#define ATA_CYL_HI		5
#define ATA_LBA_HI		5
#define ATA_DRIVE		6
#define ATA_HEAD		6
#define ATA_COMMAND		7
#define ATA_STATUS		7

// Status bits

#define ATA_STAT_ERR		0
#define ATA_STAT_DRQ		3
#define ATA_STAT_SRV		4
#define ATA_STAT_DF		5
#define ATA_STAT_RDY		6
#define ATA_STAT_BSY		7

// ATA device control port

#define ATA_CTRL_PORT       0x3F6

// Status bits

#define ATA_CTRL_NIEN		1
#define ATA_CTRL_SRST		2
#define ATA_CTRL_HOB		7

/* READ_SECTORS - 0x20
offset
0x00   - Reserved
0x01   - Count (Number of sectors to transfer. 0x00 => 256 sectors)
0x02 - LBA - Address of first logical sector. MSB
0x03 - LBA
0x04 - LBA - LSB
0x05 - Command (0x20)

*/
#define ATA_READ_SECTORS	0x20
#define ATA_READ_SECTORS_EXT	0x24
#endif /* ATA_H */
