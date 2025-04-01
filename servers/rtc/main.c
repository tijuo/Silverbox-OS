#include "rtc.h"
#include <os/syscalls.h>
#include <stdlib.h>
#include <os/services.h>
#include <os/msg/rtc.h>
#include <os/io.h>
#include <stdio.h>

#define UNIX_EPOCH          1970u
#define CENTURY_START       2000u

volatile int current_time;

const int MONTH_DAYS[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

CONST static int is_leap(int year);
CONST static int bcd_to_binary(int num);
static int get_time(void);
PURE static int get_time_in_secs(int year, int month,
    int day, int hour,
    int minute,
    int second);
static int is_update_in_progress(void);

/* A leap year is either a non-century year divisible by 4 or
 a century year divisible by 400. */

static int is_leap(int year)
{
    return (((year % 4u) == 0 && (year % 100u) != 0) || (year % 400u) == 0);
}

/* Converts a BCD-encoded number into binary. Each 4-bit nibble in a BCD-encoded number
 has a value from 0-9. */

static int bcd_to_binary(int num)
{
    return (num & 0x0Fu) + 10 * ((num & 0xF0u) >> 4) + 100 * ((num & 0xF00u) >> 8)
        + 1000 * ((num & 0xF000u) >> 12);
}

static int get_time_in_secs(int year, int month,
    int day, int hour,
    int minute, int second)
{
    year += CENTURY_START;
    int elapsed = second
        + 60 * (minute + 60 * (hour + 24 * (day + 365 * (year - UNIX_EPOCH))));

    for(unsigned i = 0; i < month; i++)
        elapsed += MONTH_DAYS[i] * 24 * 60 * 60;

    if(is_leap(year) && month > 1u)
        elapsed += 24 * 60 * 60;

    for(unsigned i = UNIX_EPOCH; i < year; i++) {
        if(is_leap(i))
            elapsed += 60 * 60 * 24;
    }

    return elapsed;
}

static int is_update_in_progress(void)
{
    out_port8(RTC_INDEX, RTC_STATUS_A);
    return (in_port8(RTC_DATA) & RTC_A_UPDATING);
}

static int get_time(void)
{
    int year, month, day, hour, minute, second;
    int bcd, _24hr;
    unsigned char statusData;

    out_port8(RTC_INDEX, RTC_STATUS_B);
    statusData = in_port8(RTC_DATA);

    bcd = (statusData & RTC_BINARY) ? 0 : 1;
    _24hr = (statusData & RTC_24_HR) ? 1 : 0;

    while(is_update_in_progress())
        ;

    out_port8(RTC_INDEX, RTC_SECOND); // second
    second = (bcd ? bcd_to_binary(in_port8(RTC_DATA)) : in_port8(RTC_DATA));

    out_port8(RTC_INDEX, RTC_MINUTE); // minute

    minute = (bcd ? bcd_to_binary(in_port8(RTC_DATA)) : in_port8(RTC_DATA));

    out_port8(RTC_INDEX, RTC_HOUR); // hour
    hour = (bcd ? bcd_to_binary(in_port8(RTC_DATA)) : in_port8(RTC_DATA));

    if(!_24hr) {
        hour--;

        if(hour & 0x80)
            hour = 12 + (hour & 0x7F);
        else
            hour = (hour & 0x7F);
    }

    out_port8(RTC_INDEX, RTC_DAY); // day
    day = (bcd ? bcd_to_binary(in_port8(RTC_DATA)) : in_port8(RTC_DATA)) - 1;

    out_port8(RTC_INDEX, RTC_MONTH); // month
    month = (bcd ? bcd_to_binary(in_port8(RTC_DATA)) : in_port8(RTC_DATA)) - 1;

    out_port8(RTC_INDEX, RTC_YEAR); // century year (00-99)
    year = (bcd ? bcd_to_binary(in_port8(RTC_DATA)) : in_port8(RTC_DATA));

    if(!is_update_in_progress())
        current_time = get_time_in_secs(year, month, day, hour, minute, second);

    return current_time;
}

int main(void)
{
    int failureCount = 0;

    fprintf(stderr, "Starting rtc server...\n");

    if(name_register(RTC_NAME) == 0)
        fprintf(stderr, "rtc name registered\n");
    else
        fprintf(stderr, "rtc name not registered.\n");

    while(1) {
        struct get_timeResponse responseBody;

        msg_t requestMsg = {
          .sender = ANY_SENDER,
          .flags = 0,
          .buffer = NULL,
          .bufferLen = 0,
        };

        failureCount = 0;

        if(sys_receive(&requestMsg) != ESYS_OK) {
            if(failureCount > 5)
                return EXIT_FAILURE;
            else
                failureCount++;
        }

        msg_t response_msg = {
          .recipient = requestMsg.sender,
          .flags = MSG_NOBLOCK,
          .buffer = &responseBody,
          .bufferLen = sizeof responseBody,
        };

        switch(requestMsg.subject) {
            case GET_TIME_MSG:
            {
                response_msg.subject = RESPONSE_OK;
                responseBody.time = get_time();
                break;
            }
            default:
                response_msg.subject = RESPONSE_FAIL;
                response_msg.buffer = NULL;
                response_msg.bufferLen = 0;
                break;
        }

        if(sys_send(&response_msg) != ESYS_OK)
            fprintf(stderr, "rtc: Failed to send response to %d\n",
                requestMsg.sender);
    }

    return EXIT_FAILURE;
}
