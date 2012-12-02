#ifndef SERIAL
#define SERIAL

#define SER1_BASE       0x3F8u // IRQ 4
#define SER2_BASE       0x2F8u // IRQ 3
#define SER3_BASE       0x3E8u // IRQ 4
#define SER4_BASE       0x2E8u // IRQ 3

#define TRANS_BUFF      0u // DLAB 0 WRITE
#define RECEIVER_BUFF   0u // DLAB 0 READ
#define DLL_BYTE        0u // DLAB 1 R/W
#define INT_ENABLE      1u // DLAB 0 R/W
#define DLH_BYTE        1u // DLAB 1 R/W
#define INT_ID          2u // READ
#define FIFO_CTRL       2u // WRITE
#define LINE_CTRL       3u // R/W
#define MODEM_CTRL      4u // R/W
#define LINE_STATUS     5u // READ
#define MODEM_STATUS    6u // READ
#define SCRATCH_REG     7u // R/W

#define BREAK_ENABLE    (0x01u << 6)

#define NO_PARITY       (0x00u << 3)
#define ODD_PARITY      (0x01u << 3)
#define EVEN_PARITY     (0x03u << 3)
#define HIGH_PARITY     (0x05u << 3)
#define LOW_PARITY      (0x07u << 3)

#define ONE_STOPBIT     (0x00u << 2)
#define TWO_STOPBITS    (0x01u << 2)

#define LENGTH_5BITS    0x00u
#define LENGTH_6BITS    0x01u
#define LENGTH_7BITS    0x02u
#define LENGTH_8BITS    0x03u

#define S50BAUD         0u
#define S300BAUD        1u
#define S600BAUD        2u
#define S2400BAUD       3u
#define S4800BAUD       4u
#define S9600BAUD       5u
#define S19200BAUD      6u
#define S39400BAUD      7u
#define S57600BAUD      8u
#define S115200BAUD     9u


#endif
