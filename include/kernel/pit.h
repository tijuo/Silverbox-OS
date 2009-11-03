#ifndef PIT
#define PIT

#define TIMER0		0x40
#define TIMER1		0x41
#define TIMER2		0x42
#define TIMER_CTRL	0x43


// Bit 0
#define BIN_COUNTER		0x00
#define BCD_COUNTER		0x01

// Bits 321

#define C_MODE0		0x00  // mode 0
#define C_MODE1		0x02  // mode 1
#define C_MODE2		0x04  // mode 2
#define C_MODE3		0x06  // mode 3
#define C_MODE4		0x08  // mode 4
#define C_MODE5		0x0A  // mode 5


// Bits 54
#define RWL_FORMAT0		0x00  // latch present counter value
#define RWL_FORMAT1		0x10  // read/write of MSB only
#define RWL_FORMAT2		0x20  // read/write of LSB only
#define RWL_FORMAT3		0x30  // read/write LSB,followed by write
// of MSB

// Bits 76

#define C_SELECT0		0x00  // Select counter 0
#define C_SELECT1       0x40  // Select counter 1
#define C_SELECT2       0x80  // Select counter 2
#define C_SELECT3		0xC0  // Select counter 3

#define TIMER_FREQ		1193182
#define HZ			   10

#endif
