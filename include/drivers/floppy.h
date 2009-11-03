#ifndef FLOPPY_DRIVER
#define FLOPPY_DRIVER

#include <types.h>

/* 82077AA Floppy Controller */

#define STATUS_REGA	        0x3F0 // Read
#define STATUS_REGB	        0x3F1 // Read
#define DIGITAL_OUT	        0x3F2 // Read/Write
#define TAPE_DRV	        0x3F3 // Read/Write
#define MAIN_STATUS	        0x3F4 // Read
#define DATARATE_SEL    	0x3F4 // Write
#define DATA_FIFO	        0x3F5 // Read/Write
#define DIGITAL_IN	        0x3F7 // Read
#define CONFIG_CTRL	        0x3F7 // Write

/* Digital Ouput Register bits */

#define DRIVE0		        0x1C
#define DRIVE1		        0x2D
/*
#define DRIVE2		        0x4E
#define DRIVE3		        0x8F
*/
#define DOR_DMA_IRQ         0x08
#define DOR_RESET           0x04

/* Tape Drive Register bits */

#define NO_TAPE		       0x00
#define TAPE1		       0x01
#define TAPE2		       0x02
#define TAPE3		       0x03

/* Data rate select bits */

#define PRE1MBPS	       (0x01 << 2)
#define PRE500KBPS	       (0x03 << 2)
#define PRE300KBPS	       PRE500KBPS
#define PRE250KBPS	       PRE300KBPS
#define PREDEFAULT	       (0x00 << 2)
#define PREDISABLED	       (0x07 << 2)

#define DR1MBPS		       0x03
#define DR500KBPS	       0x00
#define DR300KBPS	       0x01
#define DR250KBPS	       0x02

/* Status Register 0 bits */

#define SR_INTCODE	       0xC0
#define SR_SEEKEND	       0x20
#define SR_EQCHECK	       0x10
#define SR_HEADADDR	       0x04
#define SR_DRVSEL	       0x03

/* Status Register 1 bits */

#define SR_ENDCYL	       0x80
#define SR_DATAERR	       0x20
#define SR_OVRRUN	       0x10
#define SR_NODATA	       0x04
#define SR_NOTWR	       0x02
#define SR_MISADDR	       0x01

/* Status Register 2 bits */

#define SR_CONTROL	       0x40
// #define SR_DATAERR	   0x20
#define SR_WRCYL	       0x10
#define SR_BADCYL	       0x02
#define SR_MSDAT	       0x01

/* Status Register 3 bits */

#define SR_WRITEPR	        0x40
#define SR_TRACK0	        0x10
#define SR_HEADADDR	        0x04
#define SR_DRVSTAT	        0x03

/* Floppy Commands */

/* Data Transfer Commands */

#define COMM_READTRACK      0x02
#define COMM_WRITESECT      0x05
#define COMM_READSECT       0x06
#define COMM_WRITEDELSECT   0x09
#define COMM_READDELSECT    0x0C
#define COMM_FORMATTRACK    0x0D

/* Control Commands */

#define COMM_SPECIFY	    0x03
#define COMM_CHECKSTATUS    0x04
#define COMM_CALIBRATEDRV   0x07
#define COMM_CHECKINTSTAT   0x08
#define COMM_READSECTORID   0x0A
#define COMM_SEEK           0x0F
/* Extended Commands */

#define COMM_CONFIGURE      0x13
#define COMM_REGSUMMARY     0x0F
#define COMM_CONTROLLERVER  0x10
#define COMM_VERIFY         0x16
#define COMM_SEEKRELATIVE   ((0x1 << 7) | 0xF)

#define FLOPPY_BUFFER       (void *)0x80000
#define FLOPPY_READ         0
#define FLOPPY_WRITE        1
#define FLOPPY_BUFFSIZE     512
#define FLOPPY_BLKSIZE      512

enum { FLOP144MB, FLOP720KB, FLOP120MB } floppy_types;

struct drive_status {
  byte drv_ready            : 1;
  volatile byte motor_on    : 1;
  byte write_protect        : 1;
  byte dsk_changed          : 1;
  byte kill                 : 1;
  int current_cyl;
  int fdc_lock;
};

struct Floppy_Device {
  int dev;
  int type;
  int total_sec;
  int secsize;
  int heads;
  int secs_per_track;
  int gap2;
  int gap3;
  int secsz_pwr; // 128 * 2 ^ secsz_pwr == secsize
  struct drive_status status;
} floppy_dev[2];

struct Floppy_RW_Comm {
  byte drive;
  byte cyl;
  byte head; 
  byte sect;
  byte secsize;
  byte tracklen;
  byte gap3len;
  byte datlen;
};

struct Floppy_Results {
  byte st0;
  byte st1;
  byte st2;
  byte cyl;
  byte head;
  byte sect;
  byte secnum;
  int  error;
};

int floppy_open(struct file *file, int mode);
int floppy_close(struct file *file);
int floppy_write(struct file *file, void *buffer, unsigned count);
int floppy_read(struct file *file, void *buffer, unsigned count);
int floppy_seek(struct file *file, int offset, int position);

const struct Floppy_Device devlist[] = {
                                 { 0xFF, FLOP144MB, 2880, 512, 2, 18, 0x54, 0x1B, 2 },
                                 { 0xFF, FLOP720KB, 1440, 512, 2,  9, 0x54, 0x1B, 2 },
                                 { 0xFF, FLOP120MB, 2400, 512, 2, 15, 0x50, 0x2A, 2 } }; 

extern void floppy(void);
void floppy_checkint(byte *st0, byte *cyl);
void get_drv_status(byte *st3);
int floppy_wr_sector(void *buf, struct Floppy_RW_Comm *comm_param);
int floppy_rd_sector(void *buf, struct Floppy_RW_Comm *comm_param);
int start_fdc_rw(int devnum, void *buffer, unsigned long blk, int nblks, int wr_comm);
int floppy_rw(void *buffer, struct Floppy_RW_Comm *comm_param, 
              struct Floppy_Results *results, int write_comm);
int fdc_seek(int drive, int head, int cyl);
void stop_motor(int drive);
void start_motor(int drive);
void kill_motor(int drive);

#endif
