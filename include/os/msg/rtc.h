#ifndef OS_MSG_RTC_H
#define OS_MSG_RTC_H

#define GET_TIME_MSG		0
#define	RTC_NAME		"rtc"

struct GetTimeResponse
{
  unsigned int time;		// Time in seconds
};

#endif /* OS_MSG_RTC_H */
