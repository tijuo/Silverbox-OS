#ifndef KERNEL_PIT_H
#define KERNEL_PIT_H

#define TIMER0		    0x40u
#define TIMER1		    0x41u
#define TIMER2		    0x42u
#define TIMER_CTRL	    0x43u

// Bit 0
#define BIN_COUNTER		0x00u
#define BCD_COUNTER		0x01u

// Bits 321

#define C_MODE0		    0x00u  // mode 0
#define C_MODE1		    0x02u  // mode 1
#define C_MODE2		    0x04u  // mode 2
#define C_MODE3		    0x06u  // mode 3
#define C_MODE4		    0x08u  // mode 4
#define C_MODE5		    0x0Au  // mode 5

// Bits 54
#define RWL_FORMAT0		0x00u  // latch present counter value
#define RWL_FORMAT1		0x10u  // read/write of MSB only
#define RWL_FORMAT2		0x20u  // read/write of LSB only
#define RWL_FORMAT3		0x30u  // read/write LSB,followed by write
// of MSB

// Bits 76

#define C_SELECT0		0x00u  // Select counter 0
#define C_SELECT1       0x40u  // Select counter 1
#define C_SELECT2       0x80u  // Select counter 2
#define C_SELECT3		0xC0u  // Select counter 3

#define TIMER_FREQ		1193182u
#define TIMER_QUANTA_HZ    100u

#endif /* KERNEL_PIT_H */
