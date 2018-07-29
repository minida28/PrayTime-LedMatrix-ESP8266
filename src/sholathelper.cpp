#include "sholathelper.h"

#define PROGMEM_T __attribute__((section(".irom.text.template")))

#define PRINTPORT Serial
#define DEBUGPORT Serial

#define RELEASE

#define PRINT(fmt, ...)                       \
  {                                           \
    static const char pfmt[] PROGMEM_T = fmt; \
    PRINTPORT.printf_P(pfmt, ##__VA_ARGS__);  \
  }

#ifndef RELEASE
#define DEBUGLOG(fmt, ...)                    \
  {                                           \
    static const char pfmt[] PROGMEM_T = fmt; \
    DEBUGPORT.printf_P(pfmt, ##__VA_ARGS__);  \
  }
#else
#define DEBUGLOG(...)
#endif

PrayerTimes sholat;

uint8_t HOUR;
uint8_t MINUTE;
uint8_t SECOND;
uint8_t ceilHOUR;
uint8_t ceilMINUTE;
uint8_t CURRENTTIMEID, NEXTTIMEID;

char bufHOUR[3];
char bufMINUTE[3];
char bufSECOND[3];

uint32_t currentSholatTime = 0;
uint32_t nextSholatTime = 0;

char sholatTimeYesterdayArray[TimesCount][6];
char sholatTimeArray[TimesCount][6];
char sholatTimeTomorrowArray[TimesCount][6];

char bufCommonSholat[30];

char *sholatNameStr(uint8_t id)
{
  RtcDateTime dt;
  dt.InitWithEpoch32Time(localTime);

  if (dt.DayOfWeek() == 5 && id == Dhuhr)
  {
    char JUMUAH[] = "JUMAT";
    strcpy(bufCommonSholat, JUMUAH);
  }
  else
  {
    strcpy_P(bufCommonSholat, sholatName_P[id]);
  }

  return bufCommonSholat;
}

void process_sholat()
{
  DEBUGLOG("\r\n%s\r\n", __PRETTY_FUNCTION__);

  PRINT("\r\nProcess Sholat, base timestamp: %u\r\n", now);

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

  // timezone in seconds

  // location settings;
  double lat = _configLocation.latitude;
  double lon = _configLocation.longitude;
  float tZ = TimezoneFloat();

  RtcDateTime dt;

  int year;
  int month;
  int day;

  // time(&now);

  //CALCULATE YESTERDAY'S SHOLAT TIMES
  uint32_t t_yesterday = localTime - SECS_PER_DAY;

  dt.InitWithEpoch32Time(t_yesterday);

  year = dt.Year();
  month = dt.Month();
  day = dt.Day();

  sholat.get_prayer_times(year, month, day, lat, lon, tZ, sholat.timesYesterday);

  // print sholat times for yesterday
  PRINT("\r\n%s - YESTERDAY\r\n", getDateTimeStr(t_yesterday));
  for (uint8_t i = 0; i < sizeof(sholat.timesYesterday) / sizeof(double); i++)
  {
    // Convert sholat time from float to hour and minutes
    // and store to an array (to retrieve if needed)
    const char *temp = sholat.float_time_to_time24(sholat.timesYesterday[i]);
    strlcpy(sholatTimeYesterdayArray[i], temp, sizeof(sholatTimeYesterdayArray[i]));

    // re-initialize initial struct value
    // so its not get overwritten during for loop process
    dt.InitWithEpoch32Time(t_yesterday);

    // get hour and minute of sholat time
    uint8_t hr, mnt;
    sholat.get_float_time_parts(sholat.timesYesterday[i], hr, mnt);

    // modify time struct
    dt = RtcDateTime(dt.Year(), dt.Month(), dt.Day(), hr, mnt, 0);

    // revert back struct to GMT
    dt -= TimezoneSeconds();

    // and store its timestamp
    sholat.timestampSholatTimesYesterday[i] = dt.Epoch32Time();

    //Print all results
    char tmp[10];
    PRINT("%d  %-8s  %s  %s  %u\r\n",
          i,
          //TimeName[i],
          sholatNameStr(i),
          dtostrf(sholat.timesYesterday[i], 8, 5, tmp),
          // sholat.timesYesterday[i],
          sholatTimeYesterdayArray[i],
          sholat.timestampSholatTimesYesterday[i]);
    //Note:
    //snprintf specifier %f for float or double is not available in embedded device avr-gcc.
    //Therefore, float or double must be converted to string first. For this case I've used dtosrf to achive that.
  }

  // CALCULATE TODAY'S SHOLAT TIMES
  dt.InitWithEpoch32Time(localTime);

  year = dt.Year();
  month = dt.Month();
  day = dt.Day();

  sholat.get_prayer_times(year, month, day, lat, lon, tZ, sholat.times);

  // print sholat times
  PRINT("\r\n%s - TODAY\r\n", getDateTimeStr(localTime));
  for (uint8_t i = 0; i < sizeof(sholat.times) / sizeof(double); i++)
  {
    //Convert sholat time from float to hour and minutes
    //and store to an array (to retrieve if needed)
    const char *temp = sholat.float_time_to_time24(sholat.times[i]);
    strlcpy(sholatTimeArray[i], temp, sizeof(sholatTimeArray[i]));

    // re-initialize initial struct value
    // so its not get overwritten during for loop process
    dt.InitWithEpoch32Time(localTime);
    
    //Calculate timestamp of of sholat time
    uint8_t hr, mnt;
    sholat.get_float_time_parts(sholat.times[i], hr, mnt);

    // modify time struct
    dt = RtcDateTime(dt.Year(), dt.Month(), dt.Day(), hr, mnt, 0);

    // revert back struct to GMT
    dt -= TimezoneSeconds();

    // store to timestamp array
    sholat.timestampSholatTimesToday[i] = dt.Epoch32Time();

    //Print all results
    char tmp[10];
    PRINT("%d  %-8s  %s  %s  %u\r\n",
          i,
          sholatNameStr(i),
          dtostrf(sholat.times[i], 8, 5, tmp),
          // sholat.times[i],
          sholatTimeArray[i],
          sholat.timestampSholatTimesToday[i]);
    //Note:
    //snprintf specifier %f for float or double is not available in embedded device avr-gcc.
    //Therefore, float or double must be converted to string first. For this case I've used dtosrf to achive that.
  }

  // CALCULATE TOMORROW'S SHOLAT TIMES
  uint32_t t_tomorrow = localTime + SECS_PER_DAY;

  dt.InitWithEpoch32Time(t_tomorrow);

  year = dt.Year();
  month = dt.Month();
  day = dt.Day();

  sholat.get_prayer_times(year, month, day, lat, lon, tZ, sholat.timesTomorrow);

  // print sholat times for Tomorrow
  PRINT("\r\n%s - TOMORROW\r\n", getDateTimeStr(t_tomorrow));
  for (uint8_t i = 0; i < sizeof(sholat.timesTomorrow) / sizeof(double); i++)
  {
    //Convert sholat time from float to hour and minutes
    //and store to an array (to retrieve if needed)
    const char *temp = sholat.float_time_to_time24(sholat.times[i]);
    strlcpy(sholatTimeTomorrowArray[i], temp, sizeof(sholatTimeTomorrowArray[i]));

    // re-initialize initial struct value
    // so its not got overwritten during for loop process
    dt.InitWithEpoch32Time(t_tomorrow);
    
    //Calculate timestamp of of sholat time
    uint8_t hr, mnt;
    sholat.get_float_time_parts(sholat.timesTomorrow[i], hr, mnt);

    // modify time struct
    dt = RtcDateTime(dt.Year(), dt.Month(), dt.Day(), hr, mnt, 0);

    // revert back struct to GMT
    dt -= TimezoneSeconds();

    // store to timestamp array
    sholat.timestampSholatTimesTomorrow[i] = dt.Epoch32Time();

    //Print all results
    char tmp[10];
    PRINT("%d  %-8s  %s  %s  %u\r\n",
          i,
          sholatNameStr(i),
          dtostrf(sholat.timesTomorrow[i], 8, 5, tmp),
          // sholat.timesTomorrow[i],
          sholatTimeTomorrowArray[i],
          sholat.timestampSholatTimesTomorrow[i]);
    //Note:
    //snprintf specifier %f for float or double is not available in embedded device avr-gcc.
    //Therefore, float or double must be converted to string first. For this case I've used dtosrf to achive that.
  }

  PRINT("\r\n");

  //config_save_sholat("Bekasi", latitude, longitude, timezoneSholat, Egypt, Shafii, AngleBased, 20, 1, 18);
}

void process_sholat_2nd_stage()
{
  // DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  uint32_t s_tm = 0;

  // time_t t_utc;

  // time(&now);

  //int hrNextTime, mntNextTime;

  //for (unsigned int i = 0; i < sizeof(sholat.times) / sizeof(double); i++) {
  for (int i = 0; i < TimesCount; i++)
  {
    if (i != Sunset)
    {

      //First we decide, what the ID for current and next sholat time are.
      int tempCurrentID, tempPreviousID, tempNextID;

      tempCurrentID = i;
      tempPreviousID = i - 1;
      tempNextID = i + 1;

      //check NextID
      if (tempNextID == Sunset)
      {
        tempNextID = Maghrib;
      }
      if (tempCurrentID == Isha)
      {
        tempNextID = Fajr;
      }

      //check PreviousID
      if (tempPreviousID == Sunset)
      {
        tempPreviousID = Asr;
      }
      if (tempCurrentID == Fajr)
      {
        tempPreviousID = Isha;
      }

      //then
      uint32_t timestamp_current_today;
      uint32_t timestamp_next_today;
      uint32_t timestamp_next_tomorrow;

      timestamp_current_today = sholat.timestampSholatTimesToday[tempCurrentID];

      timestamp_next_today = sholat.timestampSholatTimesToday[tempNextID];

      timestamp_next_tomorrow = sholat.timestampSholatTimesTomorrow[tempNextID];

      DEBUGLOG("timestamp_current_today=%u, timestamp_next_today=%u, timestamp_next_tomorrow=%u\r\n", timestamp_current_today, timestamp_next_today, timestamp_next_tomorrow);

      if (timestamp_current_today < timestamp_next_today)
      {
        if (now <= timestamp_current_today && now < timestamp_next_today)
        {
          CURRENTTIMEID = tempPreviousID;
          NEXTTIMEID = tempCurrentID;
          s_tm = timestamp_current_today;

          break;
        }
        else if (now > timestamp_current_today && now <= timestamp_next_today)
        {
          CURRENTTIMEID = tempCurrentID;
          NEXTTIMEID = tempNextID;
          s_tm = timestamp_next_today;

          break;
        }
      }
      else if (timestamp_current_today > timestamp_next_today)
      {
        if (now >= timestamp_current_today && now < timestamp_next_tomorrow)
        {
          CURRENTTIMEID = tempCurrentID;
          NEXTTIMEID = tempNextID;
          s_tm = timestamp_next_tomorrow;

          break;
        }
      }
    }
  } //end of for loop

  DEBUGLOG("CURRENTTIMEID=%d, NEXTTIMEID=%d\r\n", CURRENTTIMEID, NEXTTIMEID);

  uint32_t timestamp_current_yesterday;
  uint32_t timestamp_current_today;
  uint32_t timestamp_next_today;
  uint32_t timestamp_next_tomorrow;

  timestamp_current_yesterday = sholat.timestampSholatTimesYesterday[CURRENTTIMEID];
  timestamp_current_today = sholat.timestampSholatTimesToday[CURRENTTIMEID];
  timestamp_next_today = sholat.timestampSholatTimesToday[NEXTTIMEID];
  timestamp_next_tomorrow = sholat.timestampSholatTimesTomorrow[NEXTTIMEID];

  if (NEXTTIMEID > CURRENTTIMEID)
  {
    currentSholatTime = timestamp_current_today;
    nextSholatTime = timestamp_next_today;
    //PRINT("%s %lu %lu\n", "Case 2a", currentSholatTime, nextSholatTime);
    DEBUGLOG("NEXTTIMEID > CURRENTTIMEID, currentSholatTime=%u, nextSholatTime=%u\r\n", currentSholatTime, nextSholatTime);
  }
  else if (NEXTTIMEID < CURRENTTIMEID)
  {
    RtcDateTime dt;
    dt.InitWithEpoch32Time(localTime); // WARNING!! DO NOT SET UTC, IT NEEDS LOCAL TIME

    if (dt.Hour() >= 12) // is PM ?
    {
      currentSholatTime = timestamp_current_today;
      nextSholatTime = timestamp_next_tomorrow;
      DEBUGLOG("NEXTTIMEID < CURRENTTIMEID, currentSholatTime=%d, nextSholatTime=%d Hour: %d, is PM\r\n", currentSholatTime, nextSholatTime, dt.Hour());
    }
    if (dt.Hour() < 12) // is AM ?
    {
      currentSholatTime = timestamp_current_yesterday;
      nextSholatTime = timestamp_next_today;
      //PRINT("%s %lu %lu\n", "Case 2c", currentSholatTime, nextSholatTime);
      DEBUGLOG("NEXTTIMEID < CURRENTTIMEID, currentSholatTime=%d, nextSholatTime=%d Hour: %d, is PM\r\n", currentSholatTime, nextSholatTime, dt.Hour());
    }
  }

  time_t timeDiff = s_tm - now;
  DEBUGLOG("s_tm: %u, now: %u, timeDiff: %li\r\n", s_tm, now, timeDiff);

  // uint16_t days;
  // uint8_t hr;
  // uint8_t mnt;
  // uint8_t sec;

  // // METHOD 2
  // // days = elapsedDays(timeDiff);
  // HOUR = numberOfHours(timeDiff);
  // MINUTE = numberOfMinutes(timeDiff);
  // SECOND = numberOfSeconds(timeDiff);

  // METHOD 3 -> https://stackoverflow.com/questions/2419562/convert-seconds-to-days-minutes-and-seconds/17590511#17590511
  tm *diff = gmtime(&timeDiff); // convert to broken down time
  // DAYS = diff->tm_yday;
  HOUR = diff->tm_hour;
  MINUTE = diff->tm_min;
  SECOND = diff->tm_sec;

  // METHOD 4 -> https://stackoverflow.com/questions/2419562/convert-seconds-to-days-minutes-and-seconds/2419597#2419597
  // HOUR = floor(timeDiff / 3600.0);
  // MINUTE = floor(fmod(timeDiff, 3600.0) / 60.0);
  // SECOND = fmod(timeDiff, 60.0);

  dtostrf(SECOND, 0, 0, bufSECOND);
  dtostrf(MINUTE, 0, 0, bufMINUTE);
  dtostrf(HOUR, 0, 0, bufHOUR);

  ceilMINUTE = MINUTE;
  ceilHOUR = HOUR;

  if (SECOND > 0)
  {
    ceilMINUTE++;
    if (ceilMINUTE == 60)
    {
      ceilHOUR++;
      ceilMINUTE = 0;
    }
  }

  if (SECOND == 0)
  {
    if (ceilHOUR != 0 || ceilMINUTE != 0)
    {
      if (ceilHOUR != 0)
      {
        PRINT("%d jam ", ceilHOUR);
      }
      if (ceilMINUTE != 0)
      {
        PRINT("%d menit ", ceilMINUTE);
      }
      PRINT("menuju %s\r\n", sholatNameStr(NEXTTIMEID));
    }
    else if (HOUR == 0 && MINUTE == 0)
    {
      PRINT("Waktu %s telah masuk!\r\n", sholatNameStr(NEXTTIMEID));
      PRINT("Dirikanlah sholat tepat waktu.\r\n");
    }
  }
}

void SholatLoop()
{
  if (now >= nextSholatTime)
  {
    process_sholat();
  }

  process_sholat_2nd_stage();

  // static int NEXTTIMEID_old = 100;

  // if (NEXTTIMEID != NEXTTIMEID_old)
  // {
  //   NEXTTIMEID_old = NEXTTIMEID;
  // }
}