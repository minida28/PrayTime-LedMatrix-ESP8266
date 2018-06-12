/*
   -------------------------------------------------------------------
   EmonESP Serial to Emoncms gateway
   -------------------------------------------------------------------
   Adaptation of Chris Howells OpenEVSE ESP Wifi
   by Trystan Lea, Glyn Hudson, OpenEnergyMonitor
   All adaptation GNU General Public License as below.

   -------------------------------------------------------------------

   This file is part of OpenEnergyMonitor.org project.
   EmonESP is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.
   EmonESP is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with EmonESP; see the file COPYING.  If not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/
#include "config.h"
#include "sholat.h"

#include <Arduino.h>
#include <EEPROM.h> // Save config settings
#include <StreamString.h>

#define DBG_OUTPUT_PORT Serial

#define RELEASE
#ifndef RELEASE
#define PROGMEM_T __attribute__((section(".irom.text.template")))
#define DEBUGLOG(fmt, ...)                         \
  {                                                \
    static const char pfmt[] PROGMEM_T = fmt;      \
    DBG_OUTPUT_PORT.printf_P(pfmt, ##__VA_ARGS__); \
  }
#else
#define DEBUGLOG(...)
#endif

// #define RELEASECONFIG

#ifndef RELEASECONFIG
#define PROGMEM_T __attribute__((section(".irom.text.template")))
#define DEBUGCONFIG(fmt, ...)                      \
  {                                                \
    static const char pfmt[] PROGMEM_T = fmt;      \
    DBG_OUTPUT_PORT.printf_P(pfmt, ##__VA_ARGS__); \
  }
#else
#define DEBUGCONFIG(...)
#endif

#define RELEASEEEPROM
#ifndef RELEASEEEPROM
#define PROGMEM_T __attribute__((section(".irom.text.template")))
#define DEBUGEEPROM(fmt, ...)                      \
  {                                                \
    static const char pfmt[] PROGMEM_T = fmt;      \
    DBG_OUTPUT_PORT.printf_P(pfmt, ##__VA_ARGS__); \
  }
#else
#define DEBUGEEPROM(...)
#endif

HUB08 matrix(A, B, C, D, OE, R1, STB, CLK);

PrayerTimes sholat;

RtcDS3231<TwoWire> Rtc(Wire);

const uint16_t ANALOGWRITE_BASE_FREQUENCY = 4000;
uint16_t ANALOGWRITE_OPERATING_FREQUENCY;

time_t lastSyncRTC;

//time_t utc_time, local_time;
time_t _lastSyncd; ///< Stored time of last successful sync
time_t _firstSync; ///< Stored time of first successful sync after boot
unsigned long _millisFirstSync;
time_t _secondsFirstSync;
time_t _lastBoot;
time_t utcTime, localTime;

// AsyncWS settings
strConfig _config;
strApConfig _configAP; // Static AP config settings
strConfigLocation _configLocation;
strConfigTime _configTime;
strHTTPAuth _httpAuth;

// Sholat settings
sholatConfig _sholatConfig;

// Led Matrix settings
ledMatrixSettings _ledMatrixSettings;
//byte currentPage;
//byte currentPage_old = 255;
byte currentPageMode0;
byte currentPageMode0_old = 255;
byte currentPageMode1;
byte currentPageMode1_old = 255;
byte currentPageMode2;
byte currentPageMode2_old = 255;
//byte OPERATING_MODE;
//byte OPERATING_MODE_old = 255;
byte MODE;
byte MODE_old = 255;
OperatingMode OPERATING_MODE_old;
bool blinkColon;

uint16_t wText, hText;

// Temp sensor variables
char bufTempSensor[10];

//NTP
bool wifiFirstConnected = false;

//RTC
time_t timeRtcNow;
char bufRtcTemp[10];
boolean RTC_OK;
boolean RTC_OK_OLD;

//Others
//float watt;
char bufWatt[10];
//float voltage;
char bufVoltage[10];

// String sholatTimeYesterdayArray[8] = {"xx:xx", "xx:xx", "xx:xx"};
//String sholatTimeArray[8] = {"xx:xx", "xx:xx", "xx:xx"};
//String sholatTimeTomorrowArray[8] = {"xx:xx", "xx:xx", "xx:xx"};

//char *sholatTimeYesterdayArrayyy[8] = {"xx:xx", "xx:xx", "xx:xx"};
char sholatTimeYesterdayArray[TimesCount][6];
char sholatTimeArray[TimesCount][6];
char sholatTimeTomorrowArray[TimesCount][6];

//char CURRENTTIMENAME[20];
//char NEXTTIMENAME[20];

char bufHOUR[2];
char bufMINUTE[2];
char bufSECOND[2];
char bufSECONDMATRIX[2];

int CURRENTTIMEID, NEXTTIMEID;
int weekday_old;
//int lenCURRENTTIMENAME, lenNEXTTIMENAME;
int HOUR;
int MINUTE;
int SECOND;
int X;

unsigned int scrollSpeed;

boolean PROCESS_SHOLAT = false;
boolean NTP_OK;
boolean syncNtpEventTriggered = false; // True if a time even has been triggered
boolean syncRtcEventTriggered = false; // True if a time even has been triggered
boolean syncSuccessByNtp = false;
boolean syncSuccessByRtc = false;

boolean stateSwitch;
boolean FORCE_UPDATE_TIME;
boolean tick500ms;
boolean tick1000ms;
boolean state250ms;
boolean state500ms;
boolean state1000ms;

const char dayNameStr0[] PROGMEM = "Err";
const char dayNameStr1[] PROGMEM = "Ahad";
const char dayNameStr2[] PROGMEM = "Senin";
const char dayNameStr3[] PROGMEM = "Selasa";
const char dayNameStr4[] PROGMEM = "Rabu";
const char dayNameStr5[] PROGMEM = "Kamis";
const char dayNameStr6[] PROGMEM = "Jumat";
const char dayNameStr7[] PROGMEM = "Sabtu";

// the short strings for each day or month must be exactly dt_SHORT_STR_LEN
#define dt_SHORT_STR_LEN 3 // the length of short strings

const char hariStr0[] PROGMEM = "Err";
const char hariStr1[] PROGMEM = "Ahad";
const char hariStr2[] PROGMEM = "Senin";
const char hariStr3[] PROGMEM = "Selasa";
const char hariStr4[] PROGMEM = "Rabu";
const char hariStr5[] PROGMEM = "Kamis";
const char hariStr6[] PROGMEM = "Jumat";
const char hariStr7[] PROGMEM = "Sabtu";

const PROGMEM char *const PROGMEM hariNames_P[] =
    {
        hariStr0, hariStr1, hariStr2, hariStr3, hariStr4, hariStr5, hariStr6, hariStr7};

const char hariShortNames_P[] PROGMEM = "ErrAHDSENSELRABKAMJUMSAB";

// TIME variables
time_t utc_time()
{
  return now();
}

time_t local_time()
{
  return now() + _configLocation.timezone / 10.0 * 3600;
}

time_t local_time(time_t t)
{
  return t + _configLocation.timezone / 10.0 * 3600;
}

time_t getUnsyncTime()
{
  return _firstSync + (millis() - _millisFirstSync) / 1000;
}

int _shortInterval; ///< Interval to set periodic time sync until first synchronization.
int _longInterval;  ///< Interval to set periodic time sync

bool setInterval(int interval)
{
  if (interval >= 15)
  {
    if (_longInterval != interval)
    {
      _longInterval = interval;
      DEBUGLOG("Sync interval set to %d\n", interval);
      if (timeStatus() == timeSet)
        setSyncInterval(interval);
    }
    return true;
  }
  else
  {
    DEBUGLOG("Sync interval set to %d second is unsuccesful\n", interval);
    DEBUGLOG("Minimum allowed sync interval is 15 seconds\n");
    return false;
  }
}

bool setInterval(int shortInterval, int longInterval)
{
  if (shortInterval >= 15 && longInterval >= shortInterval)
  {
    _shortInterval = shortInterval;
    _longInterval = longInterval;
    if (timeStatus() != timeSet)
    {
      setSyncInterval(shortInterval);
    }
    else
    {
      setSyncInterval(longInterval);
    }
    DEBUGLOG("Short sync interval set to %d\n", shortInterval);
    DEBUGLOG("Long sync interval set to %d\n", longInterval);
    return true;
  }
  else if (shortInterval < 15)
  {
    DEBUGLOG("Failed setting short sync interval to less than 15 seconds\n");
    return false;
  }
  else if (longInterval < shortInterval)
  {
    DEBUGLOG("Failed setting sync interval, long interval must be equal or higher than short interval\n");
    return false;
  }
  else
  {
    return false;
  }
}

int getInterval()
{
  return _longInterval;
}

int getShortInterval()
{
  return _shortInterval;
}

TIMESOURCE _timeSource;

#define eeprom_wifi_ssid_size sizeof(_config.ssid)
#define eeprom_wifi_password_size sizeof(_config.password)
#define eeprom_googleapikey_size 64
#define eeprom_www_username_size 32
#define eeprom_www_password_size 32

#define eeprom_size 256

#define eeprom_wifi_ssid_start 0
#define eeprom_wifi_ssid_end (eeprom_wifi_ssid_start + eeprom_wifi_ssid_size)
#define eeprom_wifi_password_start eeprom_wifi_ssid_end
#define eeprom_wifi_password_end (eeprom_wifi_password_start + eeprom_wifi_password_size)
#define eeprom_googleapikey_start eeprom_wifi_password_end
#define eeprom_googleapikey_end (eeprom_googleapikey_start + eeprom_googleapikey_size)
#define eeprom_www_username_start eeprom_googleapikey_end
#define eeprom_www_username_end (eeprom_www_username_start + eeprom_www_username_size)
#define eeprom_www_password_start eeprom_www_username_end
#define eeprom_www_password_end (eeprom_www_password_start + eeprom_www_password_size)

// -------------------------------------------------------------------
// Reset EEPROM, wipes all settings
// -------------------------------------------------------------------
void ResetEEPROM()
{
  DEBUGCONFIG("Erasing EEPROM\r\n");
  EEPROM.begin(eeprom_size);
  for (int i = 0; i < eeprom_size; ++i)
  {
    EEPROM.write(i, 0);
    DEBUGCONFIG("#");
  }
  DEBUGCONFIG("\r\n");
  EEPROM.commit();
  EEPROM.end();
}

void EEPROM_read_char(int start, int len, char *buf)
{
  // for (uint16_t i = 0; i < len; ++i)
  // {
  //   byte c = EEPROM.read(start + i);
  //   if (c != 0 && c != 255)
  //     val += (char)c;
  // }

  // //copy eeprom to buf

  EEPROM.begin(eeprom_size);

  char bufTemp[len];

  DEBUGCONFIG("EEPROM_read: ");
  for (uint8_t i = 0; i < len; i++)
  {
    byte c = EEPROM.read(start + i);

    // if (c != 0 && c != 255)
    // {
    //   temp[i] = (char)c;
    //   DEBUGCONFIG("%c", (char)c);
    // }

    bufTemp[i] = (char)c;
    DEBUGCONFIG("%c", bufTemp[i]);
  }
  bufTemp[len] = '\0';
  DEBUGCONFIG("\r\n");

  EEPROM.end();

  strlcpy(buf, bufTemp, len);
  DEBUGCONFIG("EEPROM_read temp: %s\r\n", bufTemp);
  DEBUGCONFIG("EEPROM_read buf: %s\r\n", buf);
}

void EEPROM_write_char(uint16_t start, uint8_t len, const char *buf)
{
  EEPROM.begin(eeprom_size);

  uint8_t bufLen = strlen(buf);

  DEBUGCONFIG("EEPROM_write: ");
  for (uint16_t i = 0; i < len; ++i)
  {
    if (i < bufLen)
    {
      EEPROM.write(start + i, buf[i]);
      DEBUGCONFIG("%c", buf[i]);
    }
    else
    {
      EEPROM.write(start + i, 0);
    }
  }
  DEBUGCONFIG("\r\n");

  EEPROM.commit();
  EEPROM.end();
}

// -------------------------------------------------------------------
// Load saved settings from EEPROM
// -------------------------------------------------------------------
void config_setup()
{
  // char wifi_ssid[] = "1WEG99I8A9ETN2ROT3WJN8X9EUG01234";
  // char wifi_password[] = "1WEG99I8A9ETN2ROT3WJN8X9EUG012341WEG99I8A9ETN2ROT3WJN8X9EUG56789";

  // char wifi_ssid[33] = "your_wifi_ssid";
  // char wifi_password[65] = "your_wifi_password";

  // EEPROM_write_char(eeprom_wifi_ssid_start, eeprom_wifi_ssid_size, wifi_ssid);
  // EEPROM_write_char(eeprom_wifi_password_start, eeprom_wifi_password_size, wifi_password);

  // EEPROM_read_char(eeprom_wifi_ssid_start, eeprom_wifi_ssid_size, _config.ssid);
  // EEPROM_read_char(eeprom_wifi_password_start, eeprom_wifi_password_size, _config.password);
}

void config_reset()
{
  ResetEEPROM();
}

// ****** UTF8-Decoder: convert UTF8-string to extended ASCII *******
static byte c1; // Last character buffer

// Convert a single Character from UTF8 to Extended ASCII
// Return "0" if a byte has to be ignored
byte utf8ascii(byte ascii)
{
  if (ascii < 128) // Standard ASCII-set 0..0x7F handling
  {
    c1 = 0;
    return (ascii);
  }

  // get previous input
  byte last = c1; // get last char
  c1 = ascii;     // remember actual character

  switch (last) // conversion depending on first UTF8-character
  {
  case 0xC2:
    return (ascii);
    break;
  case 0xC3:
    return (ascii | 0xC0);
    break;
  case 0x82:
    if (ascii == 0xAC)
      return (0x80); // special case Euro-symbol
  }

  return (0); // otherwise: return zero, if character has to be ignored
}

// In Place conversion UTF8-string to Extended ASCII (ASCII is shorter!)
void utf8ascii(char *s)
{
  int k = 0;
  char c;
  for (uint16_t i = 0; i < strlen(s); i++)
  {
    c = utf8ascii(s[i]);
    if (c != 0)
      s[k++] = c;
  }
  s[k] = 0;
}

void printDateTime(const RtcDateTime &dt)
{
  char datestring[20];

  snprintf_P(datestring,
             //countof(datestring),
             (sizeof(datestring) / sizeof(datestring[0])),
             PSTR("%04u/%02u/%02uT%02u:%02u:%02u"),

             dt.Year(),
             dt.Month(),
             dt.Day(),

             dt.Hour(),
             dt.Minute(),
             dt.Second()

  );

  Serial.print(datestring);
}

void process_sholat()
{
  DEBUGLOG("\n%s\r\n", __PRETTY_FUNCTION__);

  // immidiately make false
  PROCESS_SHOLAT = false;

  sholat.set_calc_method(_sholatConfig.calcMethod);
  sholat.set_asr_method(_sholatConfig.asrJuristic);
  sholat.set_high_lats_adjust_method(_sholatConfig.highLatsAdjustMethod);

  if (_sholatConfig.calcMethod == Custom)
  {
    sholat.set_fajr_angle(_sholatConfig.fajrAngle);
    //sholat.set_maghrib_angle(_sholatConfig.maghribAngle);
    sholat.set_maghrib_minutes(_sholatConfig.maghribAngle);
    sholat.set_isha_angle(_sholatConfig.ishaAngle);
  }

  // apply offset to timeOffsets array
  double timeOffsets[TimesCount] = {
      //_sholatConfig.offsetImsak,
      _sholatConfig.offsetFajr,
      _sholatConfig.offsetSunrise,
      _sholatConfig.offsetDhuhr,
      _sholatConfig.offsetAsr,
      _sholatConfig.offsetSunset,
      _sholatConfig.offsetMaghrib,
      _sholatConfig.offsetIsha};

  // Tuning SHOLAT TIME
  sholat.tune(timeOffsets);

  //CALCULATE YESTERDAY'S SHOLAT TIMES
  time_t t_yesterday = local_time() - 86400;
  sholat.get_prayer_times(t_yesterday, _configLocation.latitude, _configLocation.longitude, _configLocation.timezone / 10, sholat.timesYesterday);

  // print sholat times for Tomorrow
  Serial.println();
  digitalClockDisplay(t_yesterday);
  Serial.print(F(" - YESTERDAY's Schedule - "));
  Serial.println(hariStr(weekday(t_yesterday)));
  for (unsigned int i = 0; i < sizeof(sholat.timesYesterday) / sizeof(double); i++)
  {
    //Convert sholat time from float to hour and minutes
    //and store to an array (to retrieve if needed)
    const char *temp = sholat.float_time_to_time24(sholat.timesYesterday[i]);
    strlcpy(sholatTimeYesterdayArray[i], temp, sizeof(sholatTimeYesterdayArray[i]));

    //Calculate timestamp of of sholat time

    int hr, mnt;
    sholat.get_float_time_parts(sholat.timesYesterday[i], hr, mnt);

    //determine yesterday's year, month and day based on today's time
    TimeElements tm;
    breakTime(t_yesterday, tm);

    int yr, mo, d;
    yr = tm.Year + 1970;
    mo = tm.Month;
    d = tm.Day;

    time_t s_tm;
    s_tm = tmConvert_t(yr, mo, d, hr, mnt, 0);

    //store to timestamp array
    sholat.timestampSholatTimesYesterday[i] = s_tm;

    //Print all results
    //char tmpFloat[10];
    Serial.printf_P(PSTR("%d\t%-8s  %8.5f  %s  %d\r\n"),
                    i,
                    //TimeName[i],
                    sholatNameStr(i),
                    //dtostrf(sholat.timesYesterday[i], 8, 5, tmpFloat),
                    sholat.timesYesterday[i],
                    sholatTimeYesterdayArray[i],
                    sholat.timestampSholatTimesYesterday[i]);
    //Note:
    //snprintf specifier %f for float or double is not available in embedded device avr-gcc.
    //Therefore, float or double must be converted to string first. For this case I've used dtosrf to achive that.
  }

  // CALCULATE TODAY'S SHOLAT TIMES
  sholat.get_prayer_times(local_time(), _configLocation.latitude, _configLocation.longitude, _configLocation.timezone / 10, sholat.times);

  // print sholat times
  Serial.println();
  digitalClockDisplay();
  Serial.print(F(" - TODAY's Schedule - "));
  Serial.println(hariStr(weekday(local_time())));
  for (unsigned int i = 0; i < sizeof(sholat.times) / sizeof(double); i++)
  {
    //Convert sholat time from float to hour and minutes
    //and store to an array (to retrieve if needed)
    const char *temp = sholat.float_time_to_time24(sholat.times[i]);
    strlcpy(sholatTimeArray[i], temp, sizeof(sholatTimeArray[i]));

    //Calculate timestamp of of sholat time
    time_t s_tm;
    int hr, mnt;
    sholat.get_float_time_parts(sholat.times[i], hr, mnt);
    s_tm = tmConvert_t(year(local_time()), month(local_time()), day(local_time()), hr, mnt, 0);

    //store to timestamp array
    sholat.timestampSholatTimesToday[i] = s_tm;

    //Print all results
    //char tmpFloat[10];
    Serial.printf_P(PSTR("%d\t%-8s  %8.5f  %s  %d\r\n"),
                    i,
                    sholatNameStr(i),
                    //dtostrf(sholat.times[i], 8, 5, tmpFloat),
                    sholat.times[i],
                    sholatTimeArray[i],
                    sholat.timestampSholatTimesToday[i]);
    //Note:
    //snprintf specifier %f for float or double is not available in embedded device avr-gcc.
    //Therefore, float or double must be converted to string first. For this case I've used dtosrf to achive that.
  }

  //CALCULATE TOMORROW'S SHOLAT TIMES
  time_t t = local_time() + 86400;
  sholat.get_prayer_times(t, _configLocation.latitude, _configLocation.longitude, _configLocation.timezone / 10, sholat.timesTomorrow);

  // print sholat times for Tomorrow
  Serial.println();
  digitalClockDisplay(t);
  Serial.print(F(" - TOMORROW's Schedule - "));
  Serial.println(hariStr(weekday(t)));
  for (unsigned int i = 0; i < sizeof(sholat.timesTomorrow) / sizeof(double); i++)
  {
    //Convert sholat time from float to hour and minutes
    //and store to an array (to retrieve if needed)
    const char *temp = sholat.float_time_to_time24(sholat.times[i]);
    strlcpy(sholatTimeTomorrowArray[i], temp, sizeof(sholatTimeTomorrowArray[i]));

    //Calculate timestamp of of sholat time

    int hr, mnt;
    sholat.get_float_time_parts(sholat.timesTomorrow[i], hr, mnt);

    //determine tomorrow's year, month and day based on today's time
    time_t t = local_time() + 86400;
    TimeElements tm;
    breakTime(t, tm);

    int yr, mo, d;
    yr = tm.Year + 1970;
    mo = tm.Month;
    d = tm.Day;

    time_t s_tm;
    s_tm = tmConvert_t(yr, mo, d, hr, mnt, 0);

    //store to timestamp array
    sholat.timestampSholatTimesTomorrow[i] = s_tm;

    //Print all results
    //char tmpFloat[10];
    Serial.printf_P(PSTR("%d\t%-8s  %8.5f  %s  %d\r\n"),
                    i,
                    sholatNameStr(i),
                    //dtostrf(sholat.timesTomorrow[i], 8, 5, tmpFloat),
                    sholat.timesTomorrow[i],
                    sholatTimeTomorrowArray[i],
                    sholat.timestampSholatTimesTomorrow[i]);
    //Note:
    //snprintf specifier %f for float or double is not available in embedded device avr-gcc.
    //Therefore, float or double must be converted to string first. For this case I've used dtosrf to achive that.
  }
  Serial.println();

  //config_save_sholat("Bekasi", latitude, longitude, timezoneSholat, Egypt, Shafii, AngleBased, 20, 1, 18);
}

const char *const dayNamesStr_P[] PROGMEM =
    {
        dayNameStr0, dayNameStr1, dayNameStr2, dayNameStr3, dayNameStr4, dayNameStr5, dayNameStr6, dayNameStr7};

char bufferStr[dt_MAX_STRING_LEN + 1];

char *dayNameStr(uint8_t day)
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  strcpy_P(bufferStr, (PGM_P)(dayNamesStr_P[day]));
  return bufferStr;
}

char bufCommon[30];

char *getDateStr(time_t moment)
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  // char dateStr[12];
  // snprintf(bufCommon, sizeof(bufCommon), "%s, %d-%s-%d", dayShortStr(weekday(moment)), day(moment), monthShortStr(month(moment)), year(moment));
  snprintf(bufCommon, sizeof(bufCommon), "%s, %d-%s-%d", hariStr(weekday(moment)), day(moment), monthShortStr(month(moment)), year(moment));

  return bufCommon;
}

char *GetRtcStatusString()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  byte currentRtcStatus = RtcStatus();

  if (currentRtcStatus == RTC_TIME_VALID)
  {
    strcpy_P(bufCommon, PSTR("RTC_TIME_VALID"));
  }
  else if (currentRtcStatus == RTC_LOST_CONFIDENT)
  {
    strcpy_P(bufCommon, PSTR("RTC_LOST_CONFIDENT"));
  }
  else if (currentRtcStatus == CLOCK_NOT_RUNNING)
  {
    strcpy_P(bufCommon, PSTR("CLOCK_NOT_RUNNING"));
  }
  return bufCommon;
}

char *getDateTimeString(time_t moment)
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  strlcpy(bufCommon, getDateStr(moment), sizeof(bufCommon));
  strlcat(bufCommon, getTimeStr(moment), sizeof(bufCommon));
  return bufCommon;
}

char *getTimeStr(time_t moment)
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  //char timeStr[10];
  //sprintf (timeStr, "%02d:%02d:%02d", hour(moment), minute(moment), second(moment));
  snprintf(bufCommon, sizeof(bufCommon), "%02d:%02d:%02d", hour(moment), minute(moment), second(moment));

  return bufCommon;
}

char *getLastSyncStr()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  time_t uptime = utc_time() - _lastSyncd;

  uint16_t days;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;

  seconds = numberOfSeconds(uptime);
  uptime -= seconds;
  minutes = numberOfMinutes(uptime);
  uptime -= minutes * SECS_PER_MIN;
  //uptime -= minutesToTime_t;
  hours = numberOfHours(uptime);
  uptime -= hours * SECS_PER_HOUR;
  //uptime -= hoursToTime_t;
  days = elapsedDays(uptime);

  //char str[30];
  if (days > 0)
  {
    snprintf(bufCommon, sizeof(bufCommon), "%u day %d hr ago", days, hours);
  }
  else if (hours > 0)
  {
    snprintf(bufCommon, sizeof(bufCommon), "%d hr %d min ago", hours, minutes);
  }
  else if (minutes > 0)
  {
    snprintf(bufCommon, sizeof(bufCommon), "%d min ago", minutes);
  }
  else
  {
    snprintf(bufCommon, sizeof(bufCommon), "%d sec ago", seconds);
  }

  return bufCommon;
}

char *getNextSyncStr()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  time_t nextsync;

  if (timeStatus() == timeSet)
  {
    nextsync = _lastSyncd - utc_time() + getInterval();
  }
  else
  {
    nextsync = _lastSyncd - utc_time() + getShortInterval();
  }

  uint16_t days;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;

  //  seconds = numberOfSeconds(countdown);
  //  countdown -= seconds;
  //  minutes = numberOfMinutes(countdown);
  //  countdown -= minutes * SECS_PER_MIN;
  //  //uptime -= minutesToTime_t;
  //  hours = numberOfHours(countdown);
  //  countdown -= hours * SECS_PER_HOUR;
  //  //uptime -= hoursToTime_t;
  days = elapsedDays(nextsync);
  hours = numberOfHours(nextsync);
  minutes = numberOfMinutes(nextsync);
  seconds = numberOfSeconds(nextsync);

  //char str[30];
  snprintf(bufCommon, sizeof(bufCommon), "%u days %02d:%02d:%02d", days, hours, minutes, seconds);

  return bufCommon;
}

char *getLastBootStr()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  time_t t = _lastBoot + _configLocation.timezone / 10.0 * 3600;

  uint16_t years;
  // uint16_t months;
  uint16_t days;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;

  years = year(t);
  // months = month(t);
  days = day(t);
  hours = hour(t);
  minutes = minute(t);
  seconds = second(t);

  //char buf[30];
  snprintf(bufCommon, sizeof(bufCommon), "%d-%s-%d %02d:%02d:%02d", days, monthShortStr(month(t)), years, hours, minutes, seconds);

  return bufCommon;
}

char *getDriftStr()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  unsigned long drift = utc_time() - getUnsyncTime();

  //char str[30];
  snprintf(bufCommon, sizeof(bufCommon), "%lu", drift);

  return bufCommon;
}

char *getUptimeStr()
{
  DEBUGLOG(__PRETTY_FUNCTION__);
  DEBUGLOG("\r\n");
  //time_t uptime = utc_time() - _lastBoot;
  time_t uptime = millis() / 1000;

  uint16_t days;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;

  //  seconds = uptime % SECS_PER_MIN;
  //  uptime -= seconds;
  //  minutes = (uptime % SECS_PER_HOUR) / SECS_PER_MIN;
  //  uptime -= minutes * SECS_PER_MIN;
  //  hours = (uptime % SECS_PER_DAY) / SECS_PER_HOUR;
  //  uptime -= hours * SECS_PER_HOUR;
  //  days = uptime / SECS_PER_DAY;

  seconds = numberOfSeconds(uptime);
  uptime -= seconds;
  minutes = numberOfMinutes(uptime);
  uptime -= minutes * SECS_PER_MIN;
  //uptime -= minutesToTime_t;
  hours = numberOfHours(uptime);
  uptime -= hours * SECS_PER_HOUR;
  //uptime -= hoursToTime_t;
  days = elapsedDays(uptime);

  //char buf[30];
  snprintf(bufCommon, sizeof(bufCommon), "%u days %02d:%02d:%02d", days, hours, minutes, seconds);

  //  for (int i = 0; i < 30; i++) {
  //    bufCommon[i] = str[i];
  //  }
  //  bufCommon[30] = 0;

  return bufCommon;
}

time_t GetRtcTime()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  if (RtcStatus() == RTC_TIME_VALID)
  {
    const RtcDateTime &dt = Rtc.GetDateTime();

    time_t t = dt.Epoch32Time();

    return t;
  }

  return 0; // return 0 if unable to get the time
}

void digitalClockDisplay()
{
  // digital clock display of the time
  time_t t = now() + _configLocation.timezone / 10.0 * 3600;

  Serial.print(year(t));
  Serial.print(F("-"));
  printDigits(month(t));
  Serial.print(F("-"));
  printDigits(day(t));
  Serial.print(F("T"));
  printDigits(hour(t));
  Serial.print(F(":"));
  printDigits(minute(t));
  Serial.print(F(":"));
  printDigits(second(t));
  //Serial.println();
}

void digitalClockDisplay(time_t t)
{
  Serial.print(year(t));
  Serial.print(F("-"));
  printDigits(month(t));
  Serial.print(F("-"));
  printDigits(day(t));
  Serial.print(F("T"));
  printDigits(hour(t));
  Serial.print(F(":"));
  printDigits(minute(t));
  Serial.print(F(":"));
  printDigits(second(t));
  //Serial.println();
}

char *sholatNameStr(uint8_t id)
{
  if (weekday(localTime) == 6 && id == Dhuhr)
  {
    char JUMUAH[] = "JUMAT";
    strcpy(bufCommon, JUMUAH);
  }
  else
  {
    strcpy_P(bufCommon, sholatName_P[id]);
  }

  return bufCommon;
}

char bufferHari[dt_MAX_STRING_LEN + 1];

char *hariStr(uint8_t day)
{
  strcpy_P(bufferHari, (const char *)(*(const unsigned char **)(&(hariNames_P[day]))));
  return bufferHari;
}

char *hariShortStr(uint8_t day)
{
  uint8_t index = day * dt_SHORT_STR_LEN;
  for (int i = 0; i < dt_SHORT_STR_LEN; i++)
    bufferHari[i] = pgm_read_byte(&(hariShortNames_P[index + i]));
  bufferHari[dt_SHORT_STR_LEN] = 0;
  return bufferHari;
}

//https://forum.arduino.cc/index.php?topic=61529.msg2996010#msg2996010
time_t tmConvert_t(int YYYY, byte MM, byte DD, byte hh, byte mm, byte ss)
{
  tmElements_t tmSet;
  tmSet.Year = YYYY - 1970;
  tmSet.Month = MM;
  tmSet.Day = DD;
  tmSet.Hour = hh;
  tmSet.Minute = mm;
  tmSet.Second = ss;
  return makeTime(tmSet);
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  //Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

byte RtcStatus()
{
  DEBUGCONFIG("%s\r\n", __PRETTY_FUNCTION__);
  // status = 0; RTC time is valid.
  // status = 1; RTC lost confidence in the DateTime!
  // status = 2; Actual clock is NOT running on the RTC

  byte rtcStatus = RTC_LOST_CONFIDENT;

  if (Rtc.GetIsRunning())
  {
    if (Rtc.IsDateTimeValid())
    {
      rtcStatus = RTC_TIME_VALID;
    }
    else if (!Rtc.IsDateTimeValid())
    {
      rtcStatus = RTC_LOST_CONFIDENT;
    }
  }
  else
  {
    rtcStatus = CLOCK_NOT_RUNNING;
  }

  return rtcStatus;
}
