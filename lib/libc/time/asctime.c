#include <time.h>
#include <stdio.h>

const char *weekdays[7] = {
  "Sun",
  "Mon",
  "Tue",
  "Wed",
  "Thu",
  "Fri",
  "Sat"
};
const char *months[12] = {
  "Jan",
  "Feb",
  "Mar",
  "Apr",
  "May",
  "Jun",
  "Jul",
  "Aug",
  "Sep",
  "Oct",
  "Nov",
  "Dec"
};

char* asctime(const struct tm *timestruct) {
  static char time_buf[26];

  sprintf(time_buf, "%.3s %.3s %.2d %.2d:%.2d:%.2d %d\n",
          weekdays[timestruct->tm_wday], months[timestruct->tm_mon],
          timestruct->tm_mday, timestruct->tm_hour, timestruct->tm_min,
          timestruct->tm_sec, timestruct->tm_year + 1900);
  return time_buf;
}
