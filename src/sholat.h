/*-------------------------- In the name of God ----------------------------*\

    libprayertimes 1.0
    Islamic prayer times calculator library
    Based on PrayTimes 1.1 JavaScript library

  ----------------------------- Copyright Block --------------------------------

  Copyright (C) 2007-2010 PrayTimes.org

  Developed By: Mohammad Ebrahim Mohammadi Panah <ebrahim at mohammadi dot ir>
  Based on a JavaScript Code By: Hamid Zarrabi-Zadeh

  License: GNU LGPL v3.0

  TERMS OF USE:
    Permission is granted to use this code, with or
    without modification, in any website or application
    provided that credit is given to the original work
    with a link back to PrayTimes.org.

  This program is distributed in the hope that it will
  be useful, but WITHOUT ANY WARRANTY.

  PLEASE DO NOT REMOVE THIS COPYRIGHT BLOCK.

  ------------------------------------------------------------------------------

  User's Manual:
  http://praytimes.org/manual

  Calculating Formulas:
  http://praytimes.org/calculation

  Code Repository:
  http://code.ebrahim.ir/prayertimes/

  \*--------------------------------------------------------------------------*/

#ifndef sholat_h
#define sholat_h

//#include <cstdio>
//#include <cmath>
#include <string>
//#include <_time.h>
#include <time.h>

#define PROG_NAME "sholattimes"
#define PROG_NAME_FRIENDLY "SholatTimes"
#define PROG_VERSION "1.0"

enum
{
  VERSION_MAJOR = 1,
  VERSION_MINOR = 0,
};

typedef struct
{
  char city[48] = "KOTA BEKASI";
  int8_t timezone = 70;
  double latitude = -6.263718;
  double longitude = 106.981958;
} strConfigLocation;
extern strConfigLocation _configLocation;



// Calculation Methods
typedef enum CalculationMethod
{
  Jafari,  // Ithna Ashari
  Karachi, // University of Islamic Sciences, Karachi
  ISNA,    // Islamic Society of North America (ISNA)
  MWL,     // Muslim World League (MWL)
  Makkah,  // Umm al-Qura, Makkah
  Egypt,   // Egyptian General Authority of Survey
  Custom,  // Custom Setting

  CalculationMethodsCount
} CALCMETHOD;

// Juristic Methods
enum JuristicMethod
{
  Shafii, // Shafii (standard)
  Hanafi, // Hanafi
};

// Adjusting Methods for Higher Latitudes
enum AdjustingMethod
{
  None,       // No adjustment
  MidNight,   // middle of night
  OneSeventh, // 1/7th of night
  AngleBased, // angle/60th of night
};

typedef struct
{
  CalculationMethod calcMethod = Custom;
  JuristicMethod asrJuristic = Shafii;
  AdjustingMethod highLatsAdjustMethod = None;
  int8_t fajrAngle = 20;
  int8_t maghribAngle = 1;
  int8_t ishaAngle = 18;

  double offsetImsak = 0;
  double offsetFajr = 0;
  double offsetSunrise = 0;
  double offsetDhuhr = 0;
  double offsetAsr = 0;
  double offsetSunset = 0;
  double offsetMaghrib = 0;
  double offsetIsha = 0;

} sholatConfig;
extern sholatConfig _sholatConfig;

// Time IDs
typedef enum enumTimeID
{
  Fajr,
  Sunrise,
  Dhuhr,
  Asr,
  Sunset,
  Maghrib,
  Isha,

  TimesCount
} TimeID;

struct MethodConfig
{
  MethodConfig()
  {
  }

  MethodConfig(double fajr_angle,
               bool maghrib_is_minutes,
               double maghrib_value,
               bool isha_is_minutes,
               double isha_value)
      : fajr_angle(fajr_angle), maghrib_is_minutes(maghrib_is_minutes), maghrib_value(maghrib_value), isha_is_minutes(isha_is_minutes), isha_value(isha_value)
  {
  }

  double fajr_angle;
  bool maghrib_is_minutes;
  double maghrib_value; // angle or minutes
  bool isha_is_minutes;
  double isha_value; // angle or minutes
};

typedef std::pair<double, double> DoublePair;

/* -------------------- PrayerTimes Class --------------------- */

class PrayerTimes
{
public:
  //PrayerTimes(TimeID timeID);
  //~PrayerTimes();

  PrayerTimes(CalculationMethod calc_method = Egypt,
              JuristicMethod asr_juristic = Shafii,
              AdjustingMethod adjust_high_lats = MidNight,
              double dhuhr_minutes = 0);

  void get_prayer_times(int year, int month, int day, double _latitude, double _longitude, double _timezone, double times[]);
  void get_prayer_times(time_t date, double latitude, double longitude, double timezone, double times[]);
  static void get_float_time_parts(double time, uint8_t &hours, uint8_t &minutes);
  void set_calc_method(CalculationMethod method_id);
  void set_asr_method(JuristicMethod method_id);
  void set_high_lats_adjust_method(AdjustingMethod method_id);
  void set_fajr_angle(double angle);
  void set_maghrib_angle(double angle);
  void set_isha_angle(double angle);
  void set_dhuhr_minutes(double minutes);
  void set_maghrib_minutes(double minutes);
  void set_isha_minutes(double minutes);
  const char *float_time_to_time24(double time);
  const char *float_time_to_time12(double time, bool no_suffix = false);
  const char *float_time_to_time12ns(double time);
  static double get_effective_timezone(time_t local_time);
  static double get_effective_timezone(int year, int month, int day);
  static double time_diff(double time1, double time2);
  double times[TimesCount];
  double timesYesterday[TimesCount];
  double timesTomorrow[TimesCount];
  const char *two_digits_format(int num);
  const char *int_to_string(int num);
  uint32_t timestampSholatTimesYesterday[TimesCount];
  uint32_t timestampSholatTimesToday[TimesCount];
  uint32_t timestampSholatTimesTomorrow[TimesCount];
  uint32_t timestampPreviousSholatTime;
  uint32_t timestampCurrentSholatTime;
  uint32_t timestampNextSholatTime;
  void tune(double timeOffsets[]);

private:
  MethodConfig method_params[CalculationMethodsCount];

  //TimeID time_id;
  CalculationMethod calc_method;    // calculation method
  JuristicMethod asr_juristic;      // Juristic method for Asr
  AdjustingMethod adjust_high_lats; // adjusting method for higher latitudes
  double dhuhr_minutes;             // minutes after mid-day for Dhuhr

  double latitude;
  double longitude;
  double timezone;
  double julian_date;

  double get_julian_date(int year, int month, int day);
  double calc_julian_date(int year, int month, int day);
  void compute_times(double times[]);
  void compute_day_times(double times[]);
  void day_portion(double times[]);
  double compute_time(double g, double t);
  double compute_mid_day(double _t);
  double compute_asr(int step, double t);
  void adjust_times(double times[]);
  void adjust_high_lat_times(double times[]);
  //static double time_diff(double time1, double time2);
  double night_portion(double angle);
  static double fix_hour(double a);
  double sun_declination(double jd);

  //static double fix_hour(double a);
  static double dsin(double d);
  static double dcos(double d);
  static double dtan(double d);
  static double darcsin(double x);
  static double darccos(double x);
  static double darctan(double x);
  static double darctan2(double y, double x);
  static double darccot(double x);
  static double deg2rad(double d);
  static double rad2deg(double r);

  double equation_of_time(double jd);
  DoublePair sun_position(double jd);
  static double fix_angle(double a);

  //static String two_digits_format(int num);
  //static String int_to_string(int num);

  static const int NUM_ITERATIONS;

  double tune_times(double times[]);
  double offset[TimesCount];
};

//double times[sizeof(TimeName) / sizeof(char*)];

#endif