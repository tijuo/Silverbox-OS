#include <kernel/io.h>
#include <kernel/irq.h>
#include <kernel/interrupt.h>
#include <kernel/dma.h>
#include <kernel/task.h>
#include <kernel/syscalls.h>
#include "floppy.h"
#include <drivers/file.h>
#include <kernel/device.h>
#include <os/io.h>
#include <oslib.h>
#include <stdio.h>
#include <string.h>

#define FLOPPY_DEVICE		   3

#define POS_TO_CYL(bytes, drive)    (bytes / (floppy_dev[drive].secsize * \
                                    floppy_dev[drive].secs_per_track * \
                                    floppy_dev[drive].heads))
#define POS_TO_HEAD(bytes, drive)   ((int)(bytes / (floppy_dev[drive].secsize * \
                                    floppy_dev[drive].secs_per_track)) % \
                                    floppy_dev[drive].heads)
#define POS_TO_SECT(bytes, drive)   (((bytes / floppy_dev[drive].secsize) % \
                                    floppy_dev[drive].secs_per_track) + 1)

/* I don't think that there should be a seek command since a read or a write
   will automatically seek to the correct sector. */

volatile int motor_on;
int timeout_len;
volatile int drv_ready = 0;
int drv_sem = 1;
int comm_error;
extern int num_floppies;
//thread_t *counter;

void wait_floppy(void)
{
  while(drv_ready != 1);
//    yield();
  drv_ready = 0;
}

struct fdc_geometry {
  int cyl;
  int head;
  int sect;
  int secsize;
  int tracklen;
};

void convert_blk_to_geom(unsigned long blk, int dev, struct fdc_geometry *geom)
{
  if( geom == NULL )
    return;

  geom->sect = (blk % floppy_dev[dev].secs_per_track) + 1;
  geom->head = (blk / floppy_dev[dev].secs_per_track) % (floppy_dev[dev].heads);
  geom->cyl  = blk / (floppy_dev[dev].secs_per_track * floppy_dev[dev].heads);
  geom->secsize = floppy_dev[dev].secsize;
  geom->tracklen = floppy_dev[dev].secs_per_track;
}
/*
int floppy_request(struct blk_request *request)
{
  switch(request->command)
  {
    case REQ_READ:
      return start_fdc_rw(MINOR(request->dev), request->data, request->blk, request->numblocks, FLOPPY_READ);
      break;
    case REQ_WRITE:
      return start_fdc_rw(MINOR(request->dev), request->data, request->blk, request->numblocks, FLOPPY_WRITE);
      break;
    default:
      return -1;
  }
}
*/
int start_fdc_rw(int devnum, void *buffer, unsigned long blk, int nblks, int wr_comm)
{
  struct Floppy_RW_Comm comm_param;
  struct fdc_geometry geom;
  int i;

  if(buffer == NULL)
    return -1;

  convert_blk_to_geom(blk, devnum, &geom);
  comm_param.drive = floppy_dev[devnum].dev;
  comm_param.cyl = geom.cyl;
  comm_param.head = geom.head;
  comm_param.sect = geom.sect;

  if(floppy_dev[devnum].secsize != 0)
    comm_param.datlen = 0xFF;
  else
    return -1; // I dont know what to do with datlen if secsize equals 0

  comm_param.secsize = floppy_dev[devnum].secsz_pwr;
  comm_param.tracklen = floppy_dev[devnum].secs_per_track;
  comm_param.gap3len = floppy_dev[devnum].gap3;

  if(wr_comm) {
    for(i=0; i < nblks; i++) {
      if(floppy_wr_sector(buffer + (i * FLOPPY_BLKSIZE), &comm_param) == -1)
        return -1;
    }
  }
  else {
    for(i=0; i < nblks; i++) {
      if(floppy_rd_sector(buffer + (i * FLOPPY_BLKSIZE), &comm_param) == -1)
        return -1;
    }
  }

  return 0;
}

/*
void motor_sleep_off(void)
{
  __sleep(1800);

  if((!floppy_dev[0].status.motor_on) && (floppy_dev[0].status.kill))
    kill_motor(0);
  if((!floppy_dev[1].status.motor_on) && (floppy_dev[1].status.kill))
    kill_motor(1);

  floppy_dev[0].status.kill = floppy_dev[1].status.kill = 0;

//  kill_thread(task_current->thread_current);
}
*/

byte get_fdc_data(void) {
  int i;
  byte status, data;

  for(i=0; i < 256; i++) {
    status = inByte(MAIN_STATUS);
    if((status & 0xD0) == 0xD0) {
      data = inByte(DATA_FIFO);
      comm_error = 0;
      return data;
    }
    __sleep(2);
  }
  comm_error = 1;
//  printf("get_fdc_data(): error\n");
  return 0xFF;
}

int send_fdc_data(byte command)
{
  int i;
  byte status;

  for(i=0; i < 256; i++) {
    status = inByte(MAIN_STATUS);
    if((status & 0xC0) == 0x80) {
      outByte(DATA_FIFO, command);
      return 0;
    }
    __sleep(2);
  }
//  printf("send_fdc_data(): error\n");
  return 1;
}

void start_motor(int drive)
{
  floppy_dev[drive].status.motor_on = 1;

  switch(drive) {
    case 0:
      outByte(DIGITAL_OUT, DRIVE0);
      break;
    case 1:
      outByte(DIGITAL_OUT, DRIVE1);
      break;
/*
    case 2:
      outByte(DIGITAL_OUT, DRIVE2);
      break;
    case 3:
      outByte(DIGITAL_OUT, DRIVE3);
      break;
*/
  }
  if(floppy_dev[drive].type == FLOP120MB)
    __sleep(500); /* Motor startup delay */
  else if((floppy_dev[drive].type == FLOP720KB) ||
          (floppy_dev[drive].type == FLOP144MB))
    __sleep(300);
}

void stop_motor(int drive)
{
  floppy_dev[drive].status.motor_on = 0;
  floppy_dev[drive].status.kill = 1;
  kill_motor(drive);
//  new_thread(motor_sleep_off, task_current);
}

void kill_motor(int drive)
{ 
  switch(drive) {
    case 0:
      outByte(DIGITAL_OUT, inByte(DIGITAL_OUT) & ~DRIVE0);
      break;
    case 1:
      outByte(DIGITAL_OUT, inByte(DIGITAL_OUT) & ~DRIVE1);
      break;
/*
    case 2:
      outByte(DIGITAL_OUT, inByte(DIGITAL_OUT) & ~DRIVE2);
      break;
    case 3:
      outByte(DIGITAL_OUT, inByte(DIGITAL_OUT) & ~DRIVE3);
      break;
*/
  }
}
/*
int check_dsk_change(void)
{
  if (inByte(FDC_DIR) & 0x80) {
    dsk_changed = TRUE;
    fdc_seek(1);
    floppy_calibrate();
    stop_motor();
    return 1;
  }
  else return 0;
}

int floppy_rd_track(int drive, int cyl, int head, int sect, int secsize, 
      int tracklen, int gap3len, int datlen) 
{
  outByte(DATA_FIFO, COMM_READTRACK | (0x03 << 5));
  outByte(DATA_FIFO, drive | (head << 2));
  outByte(DATA_FIFO, cyl);
  outByte(DATA_FIFO, head);
  outByte(DATA_FIFO, sect);
  outByte(DATA_FIFO, secsize);
  outByte(DATA_FIFO, tracklen);
  outByte(DATA_FIFO, gap3len);
  outByte(DATA_FIFO, datlen);
}

int floppy_wr_sector(struct DMA_blk *blk, int drive, int cyl, int head, 
      int sect, int secsize, int tracklen, int gap3len, int datlen)
{
  start_dma(2, blk, DMA_SINGLE_MODE | DMA_AUTOINIT | DMA_WRITE_TRANS);
 
  start_motor(drive);
    
  outByte(DATA_FIFO, COMM_WRITESECT | (0x03 << 6));
  outByte(DATA_FIFO, drive | (head << 2));
  outByte(DATA_FIFO, cyl);
  outByte(DATA_FIFO, head);
  outByte(DATA_FIFO, sect);
  outByte(DATA_FIFO, secsize);
  outByte(DATA_FIFO, tracklen);
  outByte(DATA_FIFO, gap3len);
  outByte(DATA_FIFO, datlen);
  wait_floppy();
  stop_motor(drive);  
}
*/

/* Use start_fdc_rw() instead of the two functions below */

int floppy_rd_sector(void *buf, struct Floppy_RW_Comm *comm_param)
{
  struct Floppy_Results results;
  
  floppy_rw(buf, comm_param, &results, FLOPPY_READ);
  return results.error;
}

int floppy_wr_sector(void *buf, struct Floppy_RW_Comm *comm_param)
{
  struct Floppy_Results results;
  
  /* Need to check for write protect here. */
  
  floppy_rw(buf, comm_param, &results, FLOPPY_WRITE);
  return results.error;
}

/* The READ command and the WRITE command differ by only a few lines. */

int floppy_rw(void *buffer, struct Floppy_RW_Comm *comm_param,
              struct Floppy_Results *results, int write_comm)
{
  int retries, error = 0, seek_retries;

  start_motor(comm_param->drive);
  outByte(CONFIG_CTRL, DR500KBPS);

  io_wait();

  for(retries=0; retries < 3; retries++) {
    start_motor(comm_param->drive);
    for(seek_retries=0; seek_retries < 10; seek_retries++) {
      if(fdc_seek(comm_param->drive, comm_param->head,
                  comm_param->cyl) != -1)
        break;
    }
    if(seek_retries == 10)
      continue;

    if(write_comm) {
      start_dma(2, FLOPPY_BUFFER, FLOPPY_BLKSIZE, DMA_SINGLE_MODE | DMA_READ_TRANS);
      memcpy(FLOPPY_BUFFER, buffer, FLOPPY_BUFFSIZE);
      error += send_fdc_data(COMM_WRITESECT | (0x03 << 6));
    }
    else {
      start_dma(2, FLOPPY_BUFFER, FLOPPY_BLKSIZE, DMA_SINGLE_MODE | DMA_WRITE_TRANS);
      error += send_fdc_data(COMM_READSECT | (0x07 << 5)); 
    }
    if(error)
      continue;

    error += send_fdc_data(comm_param->drive | (comm_param->head << 2));
    error += send_fdc_data(comm_param->cyl);
    error += send_fdc_data(comm_param->head);
    error += send_fdc_data(comm_param->sect);
    error += send_fdc_data(comm_param->secsize);
    error += send_fdc_data(comm_param->tracklen);
    error += send_fdc_data(comm_param->gap3len);
    error += send_fdc_data(comm_param->datlen);
    
    if(error)
      continue;
    wait_floppy();
    
    results->st0 = get_fdc_data();
    results->st1 = get_fdc_data();
    results->st2 = get_fdc_data();
    results->cyl = get_fdc_data();
    results->head = get_fdc_data();
    results->sect = get_fdc_data();
    results->secnum = get_fdc_data();
      
    if((results->st0 & 0xC0) == 0) {
      stop_motor(comm_param->drive);
      results->error = 0;
      if(!write_comm)
        memcpy(buffer, FLOPPY_BUFFER, FLOPPY_BUFFSIZE);
      return 0;
    }
    else {
      stop_motor(comm_param->drive);
      continue;
    }
  }
/* Read operation failed. */
  stop_motor(comm_param->drive);
  results->error = -1;
  return -1;  
}
/*
int floppy_rddel_sector(int drive, int cyl, int head, int sect, int secsize,
      int tracklen, int gap3len, int datlen)
{
  outByte(DATA_FIFO, COMM_READDELSECT | (0x01 << 6));
  outByte(DATA_FIFO, drive | (head << 2));
  outByte(DATA_FIFO, cyl);
  outByte(DATA_FIFO, head);
  outByte(DATA_FIFO, sect);
  outByte(DATA_FIFO, secsize);
  outByte(DATA_FIFO, tracklen);
  outByte(DATA_FIFO, gap3len);
  outByte(DATA_FIFO, datlen);
}

int floppy_wrdel_sector(int drive, int cyl, int head, int sect, int secsize,
      int tracklen, int gap3len, int datlen)
{
  outByte(DATA_FIFO, COMM_WRITEDELSECT | (0x01 << 6));
  outByte(DATA_FIFO, drive | (head << 2));
  outByte(DATA_FIFO, cyl);
  outByte(DATA_FIFO, head);
  outByte(DATA_FIFO, sect);
  outByte(DATA_FIFO, secsize);
  outByte(DATA_FIFO, tracklen);
  outByte(DATA_FIFO, gap3len);
  outByte(DATA_FIFO, datlen);
}

int floppy_format_track(int drive, int head, int secsize, int tracklen,
      int gap3len, int fill)
{
  start_motor(drive);

  outByte(DATA_FIFO, COMM_FORMATTRACK | (0x01 << 6));
  outByte(DATA_FIFO, drive | (head << 2));
  outByte(DATA_FIFO, secsize);
  outByte(DATA_FIFO, tracklen);
  outByte(DATA_FIFO, gap3len);
  outByte(DATA_FIFO, fill);

  wait_floppy();
  
  stop_motor(drive);
}
*/
int floppy_calibrate(int drive)
{
  byte cyl, st0;

  start_motor(drive);

  send_fdc_data(COMM_CALIBRATEDRV);
  send_fdc_data(drive);
  wait_floppy();

  floppy_checkint(&st0, &cyl);  

  __sleep(15);

  stop_motor(drive);

  if(cyl == 0)
    return 0;
  else
    return 1;
}

void floppy_checkint(byte *st0, byte *cyl)
{
  send_fdc_data(COMM_CHECKINTSTAT);
  *st0 = get_fdc_data();
  *cyl = get_fdc_data();
  floppy_dev[0].status.current_cyl = *cyl;
}

void read_id(int drive, int head, int mfm, int *st0, int *st1, int *st2,
    int *status_cyl, int *status_head, int *status_sect, int *status_secsize)
{
  byte st0r, cyl;
  
  send_fdc_data(COMM_READSECTORID | (0x1 << 6));
  send_fdc_data(drive | (head << 2));
  wait_floppy();
  
  floppy_checkint(&st0r, &cyl);
  
  *st0 = get_fdc_data();
  *st1 = get_fdc_data();
  *st2 = get_fdc_data();
  *status_cyl = get_fdc_data();
  *status_head = get_fdc_data();
  *status_sect = get_fdc_data();
  *status_secsize = get_fdc_data();
}

/* Assumes that the floppy drive motor is already turn on. ex. Read, Write, etc... */

int fdc_seek(int drive, int head, int cyl)
{
  byte st0, intcyl;

//  start_motor(drive);
  
  send_fdc_data(COMM_SEEK);
  send_fdc_data(drive | (head << 2));
  send_fdc_data(cyl);
  wait_floppy();
  floppy_checkint(&st0, &intcyl);

  __sleep(15);

//  stop_motor(drive);

  if((cyl != intcyl) || ((st0 & 0x20) != 0x20))
    return 1;
  else
    return 0;
}

/* Returns 1 if FDC has support for extended commands */

int get_fdc_version(void)
{
  send_fdc_data(COMM_CONTROLLERVER);
  if(get_fdc_data() == 0x90)
    return 1;
  else
    return 0;
}

void get_drv_status(byte *st3)
{
  send_fdc_data(COMM_CHECKSTATUS);
  *st3 = get_fdc_data();
}

/*
void floppy_hand(task_t *current)
{
  drv_ready = 1;

  outByte(PIC1, EOI);
  outByte(PIC2, EOI);
}
*/

void set_fdc_timeout(int msecs)
{
  timeout_len = msecs;
}

void reset_fdc(void)
{
  byte head_unload, step_rate, head_load;
  byte imp_seek, fifo_en, poll_dis, fifo_thr, pre_trk;
  byte st0, cyl;
  int i;

  /* Issue a RESET to the FDC */
  enable_int();
  outByte(DIGITAL_OUT, 0);

  /* Re-enable IRQ and DMA */  

  outByte(DIGITAL_OUT, DOR_DMA_IRQ | DOR_RESET); 

  /* Set the CCR with the data rate */

  outByte(CONFIG_CTRL, DR500KBPS);   

  printf("waiting for floppy.\n");
  wait_floppy();
  printf("Done waiting.\n");
  /* Polling is enabled after reset, so we need to sense the interrupt
     for each of the available drives */

  /* ShowCMOSinfo() must run!!! */

  /* If the drives are sent the CONFIGURE command fast enough, their 
     interrupt status doesn't need to be sensed. */
  for(i=0; i < num_floppies; i++)
    floppy_checkint(&st0, &cyl); 
          
  imp_seek = 0;
  fifo_en = 1;
  poll_dis = 0;
  fifo_thr = 0;
  pre_trk = 0;
  printf("Sending a configure command.\n");
  send_fdc_data(COMM_CONFIGURE);
  send_fdc_data(0);
  send_fdc_data((imp_seek << 6) | (fifo_en << 5) | (poll_dis << 4) | (fifo_thr)); 
  send_fdc_data(pre_trk);

  /* The SPECIFY command will set the floppy step rate, head load time and head
     unload time. It will also enable/disable DMA mode */
  head_unload = 0x0F; // 240 ms
  head_load = 0x01;   // 2 ms
  step_rate = 0x0D;   // 3 ms

  send_fdc_data(COMM_SPECIFY);
  send_fdc_data((step_rate << 4) | head_unload);
  send_fdc_data((head_load << 1) | 0);

  for(i=0; (i < 2) && (floppy_dev[i].dev != 0xFF); i++) {
    floppy_dev[i].status.motor_on = 0;
    floppy_dev[i].status.kill = 0;
    floppy_dev[i].status.fdc_lock = 0;
//    floppy_calibrate(i);
  }
  comm_error = 0;
  disable_int();
}

byte get_floppies(void)
{
  byte floppies;

  outByte(0x70, 0x10);

  floppies = inByte(0x71);

  return floppies;
}

int main(void)
{
  byte floppies;

  floppies = get_floppies();

  switch(floppies >> 4) {
    case 2:
      floppy_dev[0] = devlist[2];
      floppy_dev[0].dev = 0;
      break;
    case 3:
      floppy_dev[0] = devlist[1];
      floppy_dev[0].dev = 0;
      break;
    case 4:
      floppy_dev[0] = devlist[0];
      floppy_dev[0].dev = 0;
      break;
    default:
      floppy_dev[0].dev = 0xFF;
      break;
  }

  switch(floppies & 0xF) {
    case 2:
      floppy_dev[1] = devlist[2];
      floppy_dev[1].dev = 1;
      break;
    case 3:
      floppy_dev[1] = devlist[1];
      floppy_dev[1].dev = 1;
      break;
    case 4:
      floppy_dev[1] = devlist[0];
      floppy_dev[1].dev = 1;
      break;
    default:
      floppy_dev[1].dev = 0xFF;
      break;
  }

  add_ISR(floppy, IRQ6);

//  if(register_blk_dev("floppy", FLOPPY_DEVICE, &floppy_ops, 512, (int *)floppy_request, 2) == -1)
    printf("Failed to register floppy device.");
//  else
    printf("Floppy device registered.\n");

  reset_fdc();
//  while(1);
  return 0;
}
