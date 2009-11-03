#ifndef SERIAL
#define SERIAL

#define SER1_BASE       0x3F8 // IRQ 4
#define SER2_BASE       0x2F8 // IRQ 3
#define SER3_BASE       0x3E8 // IRQ 4
#define SER4_BASE       0x2E8 // IRQ 3

#define TRANS_BUFF      0 // DLAB 0 WRITE
#define RECEIVER_BUFF   0 // DLAB 0 READ
#define DLL_BYTE        0 // DLAB 1 R/W
#define INT_ENABLE      1 // DLAB 0 R/W
#define DLH_BYTE        1 // DLAB 1 R/W
#define INT_ID          2 // READ
#define FIFO_CTRL       2 // WRITE
#define LINE_CTRL       3 // R/W
#define MODEM_CTRL      4 // R/W
#define LINE_STATUS     5 // READ
#define MODEM_STATUS    6 // READ
#define SCRATCH_REG     7 // R/W

#define BREAK_ENABLE    (0x01 << 6)

#define NO_PARITY       (0x00 << 3)
#define ODD_PARITY      (0x01 << 3)
#define EVEN_PARITY     (0x03 << 3)
#define HIGH_PARITY     (0x05 << 3)
#define LOW_PARITY      (0x07 << 3)

#define ONE_STOPBIT     (0x00 << 2)
#define TWO_STOPBITS    (0x01 << 2)

#define LENGTH_5BITS    0x00
#define LENGTH_6BITS    0x01
#define LENGTH_7BITS    0x02
#define LENGTH_8BITS    0x03

#define S50BAUD         0
#define S300BAUD        1
#define S600BAUD        2
#define S2400BAUD       3
#define S4800BAUD       4
#define S9600BAUD       5
#define S19200BAUD      6
#define S39400BAUD      7
#define S57600BAUD      8
#define S115200BAUD     9


#endif
