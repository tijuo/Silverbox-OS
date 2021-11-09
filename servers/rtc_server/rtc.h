#ifndef RTC
#define RTC

#define RTC_INDEX           0x70
#define RTC_DATA            0x71

#define RTC_SECOND          0x00    // (0-59)
#define RTC_ALRM_SECOND     0x01
#define RTC_MINUTE          0x02    // (0-59)
#define RTC_ALRM_MINUTE     0x03
#define RTC_HOUR            0x04    // (0-23 24hr mode, 1-12 12 hr mode high 
                                    // bit unset/set for am/pm)
#define RTC_ALRM_HOUR       0x05
#define RTC_WEEKDAY         0x06    // (1-7, Sunday=1)
#define RTC_DAY             0x07    // (1-31)
#define RTC_MONTH           0x08    // (1-12)
#define RTC_YEAR            0x09    // (00-99)
#define RTC_STATUS_A        0x0a

/* RTC Status Register A
Bits:
      0-3 - Rate selection bits for divider output(initially 0110 = 1.024 kHz)
      4-6 - 22 stage divider, time base used; (initially 010 = 32.768 kHz)
       7  - 1=time update in progress, 0=time/date available
*/

#define RTC_A_UPDATING            0x80

#define RTC_STATUS_B        0x0b

/* RTC Status Register B
Bits:
      0 - 1=enable daylight savings, 0=disable(default)
      1 - 1=24 hr mode, 0=12 hour mode (24 default)
      2 - 1=time/date in binary, 0=BCD (BCD default)
      3 - 1=enable square wave freq, 0=disable
      4 - 1=enable update ended freq, 0=disable
      5 - 1=enable alarm interrupt, 0=disable
      6 - 1=enable periodic interrupt, 0=disable
      7 - 1=disable clock update, 0=update count normally

*/

#define RTC_DST_ENABLE    0x01
#define RTC_24_HR         0x02
#define RTC_BINARY        0x04
#define RTC_PERIODIC      0x40

#define RTC_STATUS_C        0x0c
#define RTC_STATUS_D        0x0d

/*
 Numbers are stored in BCD/binary if bit 2 of byte 11 is unset/set
   15  * The clock is in 12hr/24hr mode if bit 1 of byte 11 is unset/set
*/

#endif /* RTC */
