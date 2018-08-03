#include <Arduino.h>
#include <time.h>
#include "timehelper.h"
#include "rtchelper.h"
#include "EspGoodies.h"

#define PRINTPORT Serial
#define DEBUGPORT Serial

#define PRINT(fmt, ...)                   \
  {                                          \
    static const char pfmt[] PROGMEM = fmt;  \
    PRINTPORT.printf_P(pfmt, ##__VA_ARGS__); \
  }

#define RELEASE

#ifndef RELEASE
#define DEBUGLOG(fmt, ...)                   \
  {                                          \
    static const char pfmt[] PROGMEM = fmt;  \
    DEBUGPORT.printf_P(pfmt, ##__VA_ARGS__); \
  }
#define DEBUGLOGLN(fmt, ...)                 \
  {                                          \
    static const char pfmt[] PROGMEM = fmt;  \
    static const char rn[] PROGMEM = "\r\n"; \
    DEBUGPORT.printf_P(pfmt, ##__VA_ARGS__); \
    DEBUGPORT.printf_P(rn);                  \
  }
#else
#define DEBUGLOG(...)
#define DEBUGLOGLN(...)
#endif

bool tick1000ms = false;
bool tick3000ms = false;
bool state500ms = false;
bool state1000ms = false;
bool state250ms = false;

uint32_t now_ms, now_us;
timeval tv;
timespec tp;
timeval cbtime; // time set in callback
bool timeSetFlag;
bool y2k38mode = false;

uint32_t now;
uint32_t localTime;
uint32_t uptime;
uint32_t lastSync;   ///< Stored time of last successful sync
uint32_t _firstSync; ///< Stored time of first successful sync after boot
uint32_t _lastBoot;

uint16_t yearLocal;
uint8_t monthLocal;
uint8_t mdayLocal;
uint8_t wdayLocal;
uint8_t hourLocal;
uint8_t minLocal;
uint8_t secLocal;

uint16_t syncInterval;      ///< Interval to set periodic time sync
uint16_t shortSyncInterval; ///< Interval to set periodic time sync until first synchronization.
uint16_t longSyncInterval;  ///< Interval to set periodic time sync

Ticker state250msTimer;
Ticker state500msTimer;

strConfigTime _configTime;
strTimeSource timeSource;

float TimezoneFloat()
{
  time_t rawtime;
  char buffer[6];

  if (!y2k38mode)
  {
    rawtime = time(nullptr);
  }
  else if (y2k38mode)
  {
    rawtime = time(nullptr) + 2145916800 + 3133696;
  }

  strftime(buffer, 6, "%z", localtime(&rawtime));

  char bufTzHour[4];
  strlcpy(bufTzHour, buffer, sizeof(bufTzHour));
  int8_t hour = atoi(bufTzHour);

  char bufTzMin[4];
  bufTzMin[0] = buffer[0]; // sign
  bufTzMin[1] = buffer[3];
  bufTzMin[2] = buffer[4];
  bufTzMin[3] = 0;
  float min = atoi(bufTzMin) / 60.0;

  float TZ_FLOAT = hour + min;
  return TZ_FLOAT;
}

int32_t TimezoneMinutes()
{
  return TimezoneFloat() * 60;
}

int32_t TimezoneSeconds()
{
  return TimezoneMinutes() * 60;
}

char *getDateStr(uint32_t moment)
{
  // output format: Fri 08 Jun 1979
  static char buf[16];

  RtcDateTime dt;
  dt.InitWithEpoch32Time(moment);

  snprintf_P(
      buf,
      (sizeof(buf)),
      PSTR("%s %d %s %d"),
      dayShortStr(dt.DayOfWeek()),
      dt.Day(),
      monthShortStr(dt.Month()),
      dt.Year());

  return buf;
}

char *getTimeStr(uint32_t moment)
{
  // output format: 15:57:96
  static char buf[9];

  RtcDateTime dt;
  dt.InitWithEpoch32Time(moment);

  snprintf_P(
      buf,
      (sizeof(buf)),
      PSTR("%02d:%02d:%02d"),
      dt.Hour(),
      dt.Minute(),
      dt.Second());

  return buf;
}

char *getDateTimeStr(uint32_t moment)
{
  // output: Tue Jul 24 2018 16:46:44 GMT
  static char buf[29];

  RtcDateTime dt;
  dt.InitWithEpoch32Time(moment);

  snprintf_P(
      buf,
      (sizeof(buf)),
      PSTR("%s %s %2d %02d:%02d:%02d %d"),
      dayShortStr(dt.DayOfWeek()),
      monthShortStr(dt.Month()),
      dt.Day(),
      dt.Hour(),
      dt.Minute(),
      dt.Second(),
      dt.Year());

  return buf;
}

char *getLastBootStr()
{
  // output: Tue Jul 24 2018 16:46:44 GMT
  static char buf[29];

  uint32_t lastBoot = now - uptime;

  RtcDateTime dt;
  dt.InitWithEpoch32Time(lastBoot);

  snprintf_P(
      buf,
      (sizeof(buf)),
      PSTR("%s %d %s %d %02d:%02d:%02d GMT"),
      dayShortStr(dt.DayOfWeek()),
      dt.Day(),
      monthShortStr(dt.Month()),
      dt.Year(),
      dt.Hour(),
      dt.Minute(),
      dt.Second());

  return buf;
}

char *getUptimeStr()
{
  // format: 365000 days 23:47:22

  uint16_t days;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;

  days = elapsedDays(uptime);
  hours = numberOfHours(uptime);
  minutes = numberOfMinutes(uptime);
  seconds = numberOfSeconds(uptime);

  static char buf[21];
  snprintf(buf, sizeof(buf), "%d days %02d:%02d:%02d", days, hours, minutes, seconds);

  return buf;
}

char *getLastSyncStr()
{
  // format: 365000 days 23 hrs ago

  time_t diff = now - lastSync;

  uint16_t days;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;

  days = elapsedDays(diff);
  hours = numberOfHours(diff);
  minutes = numberOfMinutes(diff);
  seconds = numberOfSeconds(diff);

  static char buf[23];
  if (days > 0)
  {
    snprintf(buf, sizeof(buf), "%u day %d hr ago", days, hours);
  }
  else if (hours > 0)
  {
    snprintf(buf, sizeof(buf), "%d hr %d min ago", hours, minutes);
  }
  else if (minutes > 0)
  {
    snprintf(buf, sizeof(buf), "%d min ago", minutes);
  }
  else
  {
    snprintf(buf, sizeof(buf), "%d sec ago", seconds);
  }

  return buf;
}

char *getNextSyncStr()
{
  // format: 365000 days 23:47:22

  time_t _syncInterval = 3601;

  time_t diff = (lastSync + _syncInterval) - now;

  uint16_t days;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;

  days = elapsedDays(diff);
  hours = numberOfHours(diff);
  minutes = numberOfMinutes(diff);
  seconds = numberOfSeconds(diff);

  static char buf[21];
  snprintf(buf, sizeof(buf), "%d days %02d:%02d:%02d", days, hours, minutes, seconds);

  return buf;
}

char *GetRtcDateTimeStr(const RtcDateTime &dt)
{
  // output format: Sat Jul 21 2018 10:59:32 GMT
  static char buf[29];

  snprintf_P(buf,
             //countof(datestring),
             (sizeof(buf) / sizeof(buf[0])),
             PSTR("%s %s %2d %02d:%02d:%02d %d"),

             dayShortStr(dt.DayOfWeek()),
             monthShortStr(dt.Month()),
             dt.Day(),
             dt.Hour(),
             dt.Minute(),
             dt.Second(),
             dt.Year());

  return buf;
}

void FlipState250ms()
{
  //  static boolean state250ms;
  state250ms = !state250ms;
}

void FlipState500ms()
{
  state500ms = !state500ms;
}

#define PTM(w)              \
  Serial.print(":" #w "="); \
  Serial.print(tm->tm_##w);

void printTm(const char *what, const tm *tm)
{
  Serial.print(what);
  PTM(isdst);
  PTM(yday);
  PTM(wday);
  PTM(year);
  PTM(mon);
  PTM(mday);
  PTM(hour);
  PTM(min);
  PTM(sec);
}

void time_is_set()
{
  gettimeofday(&cbtime, NULL);
  // SetRtcTime(time(nullptr));
  timeSetFlag = true;
  y2k38mode = false;
  PRINT("\r\n------------------ settimeofday() was called ------------------\r\n\r\n");

  clock_gettime(0, &tp);
  // now = time(nullptr);
  DEBUGLOG("time(nullptr): %u, (uint32_t)tv.tv_sec: %u", time(nullptr), (uint32_t)tv.tv_sec)
}

void TimeSetup()
{
  state250msTimer.attach(0.25, FlipState250ms);

  settimeofday_cb(time_is_set);

  // configTZ(TZ_Asia_Jakarta);
  // configTZ(_configLocation.timezonestring);
  // configTZ("WIB-7");
  setenv("TZ", _configLocation.timezonestring, 1/*overwrite*/);
  tzset();

  if (_configTime.enablentp)
  {
    configTime(0, 0, _configTime.ntpserver_0, _configTime.ntpserver_1, _configTime.ntpserver_2);
  }

  if (_configTime.enablertc)
  {
    uint32_t rtc = get_time_from_rtc();
    timeval tv = {rtc, 0};
    timezone tz = {0, 0};
    settimeofday(&tv, &tz);
  }
}

void TimeLoop()
{
  gettimeofday(&tv, nullptr);
  clock_gettime(0, &tp);

  now = time(nullptr);

  if (time(nullptr) < 0)
  {
    y2k38mode = true;
  }

  localTime = now + TimezoneSeconds();
  uptime = tp.tv_sec;
  now_ms = millis();
  now_us = micros();

  RtcDateTime dt;
  dt.InitWithEpoch32Time(localTime);
  yearLocal = dt.Year();
  monthLocal = dt.Month();
  mdayLocal = dt.Day();
  wdayLocal = dt.DayOfWeek();
  hourLocal = dt.Hour();
  minLocal = dt.Minute();
  secLocal = dt.Second();

  // localtime / gmtime every second change
  static uint32_t lastv = 0;
  if (lastv != now)
  {
    lastv = now;
    DEBUGLOGLN("tv.tv_sec:%li", tv.tv_sec);

    state500ms = true;
    state500msTimer.once(0.5, FlipState500ms);

    tick1000ms = true;

    if (!(now % 2))
    {
      // do something even
      state1000ms = true;
    }
    else
    {
      state1000ms = false;
    }

    static uint8_t counter3 = 0;
    counter3++;
    if (counter3 >= 3)
    {
      counter3 = 0;
      tick3000ms = true;
    }

#ifndef RELEASE
    time_t test;
    if (!y2k38mode)
    {
      test = time(nullptr);
    }
    else if (y2k38mode)
    {
      test = time(nullptr) + 2145916800 + 3133696;
    }

    Serial.println();
    printTm("localtime", localtime(&test));
    Serial.println();
    printTm("gmtime   ", gmtime(&test));
    Serial.println();

    // time from boot
    Serial.print("clock:");
    Serial.print((uint32_t)tp.tv_sec);
    Serial.print("/");
    Serial.print((uint32_t)tp.tv_nsec);
    Serial.print("ns");

    // time from boot
    Serial.print(" millis:");
    Serial.print(now_ms);
    Serial.print(" micros:");
    Serial.print(now_us);

    // EPOCH+tz+dst
    Serial.print(" gtod:");
    Serial.print((uint32_t)tv.tv_sec);
    Serial.print("/");
    Serial.print((uint32_t)tv.tv_usec);
    Serial.print("us");

    // EPOCH+tz+dst
    Serial.print(" time_t:");
    Serial.print(now);
    Serial.print(" time uint32_t:");
    Serial.println((uint32_t)now);

    // RtcDateTime timeToSetToRTC;
    // timeToSetToRTC.InitWithEpoch32Time(now);
    // Rtc.SetDateTime(timeToSetToRTC);

    // human readable
    // Printed format: Wed Oct 05 2011 16:48:00 GMT+0200 (CEST)
    char buf[60];
    // output: Wed Jan 01 1902 07:02:04 GMT+0700 (WIB)
    // strftime(buf, sizeof(buf), "%a %b %d %Y %X GMT", gmtime(&test));
    strftime(buf, sizeof(buf), "%c GMT", gmtime(&test));
    DEBUGLOGLN("NTP GMT   date/time: %s", buf);
    // strftime(buf, sizeof(buf), "%a %b %d %Y %X GMT%z (%Z)", localtime(&test));
    strftime(buf, sizeof(buf), "%c GMT%z (%Z)", localtime(&test));
    DEBUGLOGLN("NTP LOCAL date/time: %s", buf);

    RtcDateTime dt;
    dt.InitWithEpoch32Time(now);
    DEBUGLOGLN("NTP GMT   date/time: %s GMT", getDateTimeStr(now));
    dt.InitWithEpoch32Time(localTime);
    strftime(buf, sizeof(buf), "GMT%z (%Z)", localtime(&test));
    DEBUGLOGLN("NTP LOCAL date/time: %s %s", getDateTimeStr(localTime), buf);

    dt = Rtc.GetDateTime();
    DEBUGLOGLN("RTC GMT   date/time: %s GMT\r\n", GetRtcDateTimeStr(dt));
#endif
  }
}
