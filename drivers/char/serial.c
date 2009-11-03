/* serial.c
*
* Copyright (C) 2005 Tiju Oliver
*
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation; either version 2 of the License, or (at your option) any later
* version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE.
*
* For more info, look at the COPYING file.
*/
#include <oslib.h>
#include <serial.h>
#include <dev_interface.h>
#include <os/io.h>
#include <string.h>

void set_dlab(byte bit, word port);
void serialHandler();
void ser_write_buffer(byte data, word port);
void ser_enable_int(byte data, word port);
void set_modem_ctl(byte data, word port);
void ser_fifo_ctrl(byte data, word port);
void set_line_ctrl(byte data, word port);
int set_baudrate(int baud, word port);

#define IRQ4 0x24
#define IRQ3 0x23

int printMsg( char *msg )
{
  return deviceWrite( 5, 0, 0, strlen(msg), msg, strlen(msg) );
}

void scan_serports(void)
{
  word *port_addr = (word *)0x400;
  int i, io, irq;
  mid_t mbox = __allocMbox( 50 );

  i = 0;
/*
  for(i=0; i < 4; i++, port_addr++)
  {
    if(*port_addr != 0) {
      switch(i) {
        case 0:
          io = SER1_BASE;
          irq = 4;
          break;
        case 1:
          io = SER2_BASE;
          irq = 3;
          break;
        case 2:
          io = SER3_BASE;
          irq = 4;
          break;
        case 3:
          io = SER4_BASE;
          irq = 3;
          break;
        default:
          break;
      }

      print("SER%d: found. Addr: 0x%x IRQ: 0x%02x\n", i + 1, io, irq);
/ *
      if((i == 0) || (i == 2)) {
        register_irq(IRQ4);
        add_ISR(&serial, IRQ4);
      }
      else {
        register_irq(IRQ3);
        add_ISR(&serial, IRQ3);
      }
* /
    }
  }
*/
  __sleep( 1000 );

  __registerInt(mbox, IRQ4);
  __registerInt(mbox, IRQ3);

  set_dlab(1, SER1_BASE);
  set_baudrate(S9600BAUD, SER1_BASE);
  set_line_ctrl(0x03, SER1_BASE);
  ser_fifo_ctrl(0xC7, SER1_BASE);
  set_modem_ctl(0x0B, SER1_BASE);
  ser_enable_int(0x07, SER1_BASE);

  serialHandler();
}

void serialHandler()
{
  struct MessageHeader header;

  while( true )
  {
    receive( 50, NULL, 0, &header );

    if( header.subject == 0x23 )
    {
      printMsg("IRQ 3 received.\n");
      __endIRQ( 0x23 );
    }
    else
    {
      printMsg("IRQ 4 received.\n");
      __endIRQ( 0x24 );
    }
  }
}

byte ser_read_buffer(word port)
{
  set_dlab(0, port);
  return inByte(port + RECEIVER_BUFF);
}

void ser_write_buffer(byte data, word port)
{
  set_dlab(0, port);
  outByte(port + TRANS_BUFF, data);
}

void set_dlab(byte bit, word port)
{
  bit &= 1;
  outByte(LINE_CTRL + port, (inByte(LINE_CTRL + port) & 0x7F)
     | (bit << 7));
}

void set_line_ctrl(byte data, word port)
{
  outByte(LINE_CTRL + port, data);
}

void ser_enable_int(byte data, word port)
{
  set_dlab(0, port);
  outByte(port + INT_ENABLE, data);
}

void ser_fifo_ctrl(byte data, word port)
{
  outByte(port + FIFO_CTRL, data);
}

byte ser_line_stat(word port)
{
  return inByte(port + LINE_STATUS);
}

byte ser_read_intid(word port)
{
  return inByte(port + INT_ID);
}

void set_modem_ctl(byte data, word port)
{
  outByte( port + MODEM_CTRL, data );
}

int set_baudrate(int baud, word port)
{
  byte dll, dlh;

  switch(baud) {
    case S50BAUD:
      dll = 0x00;
      dlh = 0x09;
      break;
    case S300BAUD:
      dll = 0x80;
      dlh = 0x01;
      break;
    case S600BAUD:
      dll = 0xC0;
      dlh = 0x00;
      break;
    case S2400BAUD:
      dll = 0x30;
      dlh = 0x00;
      break;
    case S4800BAUD:
      dll = 0x18;
      dlh = 0x00;
      break;
    case S9600BAUD:
      dll = 0x0C;
      dlh = 0x00;
      break;
    case S19200BAUD:
      dll = 0x06;
      dlh = 0x00;
      break;
    case S39400BAUD:
      dll = 0x03;
      dlh = 0x00;
      break;
    case S57600BAUD:
      dll = 0x02;
      dlh = 0x00;
      break;
    case S115200BAUD:
      dll = 0x01;
      dlh = 0x00;
      break;
    default:
      return -1;
  }

  set_dlab(1, port);
  outByte(DLL_BYTE + port, dll);
  outByte(DLH_BYTE + port, dlh);
  return 0;
}

int main(void)
{
  __chioperm( 1, SER1_BASE, 16 );
  scan_serports();
}
