#include "rtc.h"
#include <os/syscalls.h>
#include <stdlib.h>
#include <os/services.h>
#include <os/msg/rtc.h>
#include <os/io.h>

#define UNIX_EPOCH          1970u
#define CENTURY_START       2000u

volatile int currentTime;

const unsigned int monthDays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static int PURE(isLeap(unsigned int year));
static unsigned int PURE(bcd2bin(unsigned int num));
static unsigned int getTime(void);
static unsigned int PURE(getTimeInSecs(unsigned int year, unsigned int month,
    unsigned int day, unsigned int hour, unsigned int minute, unsigned int second));
static int isUpdateInProgress(void);

extern void print(const char *);
extern void printInt(int);

/* A leap year is either a non-century year divisible by 4 or
   a century year divisible by 400. */

static int isLeap(unsigned int year)
{
  return ( ((year % 4u) == 0 && (year % 100u) != 0) ||
      (year % 400u) == 0 );
}

/* Converts a BCD-encoded number into binary. Each 4-bit nibble in a BCD-encoded number
   has a value from 0-9. */

static unsigned int bcd2bin(unsigned int num)
{
  return (num & 0x0Fu) + 10 * ((num & 0xF0u) >> 4) + 100 * ((num & 0xF00u) >> 8)
    + 1000 * ((num & 0xF000u) >> 12);
}

static unsigned int getTimeInSecs(unsigned int year, unsigned int month,
    unsigned int day, unsigned int hour, unsigned int minute, unsigned int second)
{
  year += CENTURY_START;
  unsigned int elapsed = second + 60*(minute + 60*(hour + 24*(day + 365*(year-UNIX_EPOCH))));

  for(unsigned i=0; i < month; i++)
    elapsed += monthDays[i]*24*60*60;

  if(isLeap(year) && month > 1u)
    elapsed += 24*60*60;

  for(unsigned i=UNIX_EPOCH; i < year; i++)
  {
    if(isLeap(i))
      elapsed += 60 * 60 * 24;
  }

  return elapsed;
}

static int isUpdateInProgress(void)
{
  outByte(RTC_INDEX, RTC_STATUS_A);
  return (inByte(RTC_DATA) & RTC_A_UPDATING);
}

static unsigned int getTime(void)
{
  unsigned int year, month, day, hour, minute, second;
  unsigned int bcd, _24hr;
  unsigned char statusData;

  outByte( RTC_INDEX, RTC_STATUS_B );
  statusData = inByte( RTC_DATA );

  bcd = (statusData & RTC_BINARY) ? 0 : 1;
  _24hr = (statusData & RTC_24_HR) ? 1 : 0;

  while(isUpdateInProgress());

  outByte( RTC_INDEX, RTC_SECOND ); // second
  second = (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA ));

  outByte( RTC_INDEX, RTC_MINUTE ); // minute

  minute = (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA ));

  outByte( RTC_INDEX, RTC_HOUR ); // hour
  hour = (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA ));

  if( !_24hr )
  {
    hour--;

    if( hour & 0x80 )
      hour = 12 + (hour & 0x7F);
    else
      hour = (hour & 0x7F);
  }

  outByte( RTC_INDEX, RTC_DAY ); // day
  day = (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA )) - 1;

  outByte( RTC_INDEX, RTC_MONTH ); // month
  month = (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA )) - 1;

  outByte( RTC_INDEX, RTC_YEAR ); // century year (00-99)
  year = (bcd ? bcd2bin(inByte( RTC_DATA )) : inByte( RTC_DATA ));

  if(!isUpdateInProgress())
    currentTime = getTimeInSecs(year, month, day, hour, minute, second);

  return currentTime;
}

int main(void)
{
  msg_t msg;
  msg_t responseMsg;

  registerServer(SERVER_GENERIC);
  registerName(RTC_NAME);

  while(1)
  {
    msg.sender = ANY_SENDER;

    if(sys_receive(&msg, 1) != ESYS_OK)
      return EXIT_FAILURE;

    responseMsg.recipient = msg.sender;

    switch(msg.subject)
    {
      case GET_TIME_MSG:
      {
        struct GetTimeResponse *responseBody = (struct GetTimeResponse *)&responseMsg.data;
        responseMsg.subject = RESPONSE_OK;
        responseBody->time = getTime();
        break;
      }
      default:
        responseMsg.subject = RESPONSE_FAIL;
        break;
    }

    if(sys_send(&responseMsg, 0) != ESYS_OK)
      print("rtc: Failed to send response to "), printInt(msg.sender), print("\n");
  }

  return EXIT_FAILURE;
}
