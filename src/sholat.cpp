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

#include "Arduino.h"
#include "sholat.h"

#include <cstdio>
#include <cmath>
#include <string>
#include <ctime>
#include <cmath>
#include <cstring>
#include <unistd.h>
#include <getopt.h>


/* -------------------- Interface Functions -------------------- */
/*
  PrayerTimes(CalculationMethod calc_method = Jafari,
      JuristicMethod asr_juristic = Shafii,
      AdjustingMethod adjust_high_lats = MidNight,
      double dhuhr_minutes = 0)

  get_prayer_times(date, latitude, longitude, timezone, &times)
  get_prayer_times(year, month, day, latitude, longitude, timezone, &times)

  set_calc_method(method_id)
  set_asr_method(method_id)
  set_high_lats_adjust_method(method_id)    // adjust method for higher latitudes

  set_fajr_angle(angle)
  set_maghrib_angle(angle)
  set_isha_angle(angle)
  set_dhuhr_minutes(minutes)    // minutes after mid-day
  set_maghrib_minutes(minutes)    // minutes after sunset
  set_isha_minutes(minutes)   // minutes after maghrib

  get_float_time_parts(time, &hours, &minutes)
  float_time_to_time24(time)
  float_time_to_time12(time)
  float_time_to_time12ns(time)
*/

//double PrayerTimes::times[TimesCount];
//double PrayerTimes::times[sizeof(TimeName) / sizeof(char*)];

//void PrayerTimes::TIMES(TimeID _time_id)
//{
//  times[8];
//}


//double PrayerTimes::times[TimeID time_id];

//void PrayerTimes::TIMES()
//{
//  times[sizeof(TimeName) / sizeof(char*)];
//};


PrayerTimes::PrayerTimes(CalculationMethod calc_method,
                         JuristicMethod asr_juristic,
                         AdjustingMethod adjust_high_lats,
                         double dhuhr_minutes)
  : calc_method(calc_method)
  , asr_juristic(asr_juristic)
  , adjust_high_lats(adjust_high_lats)
  , dhuhr_minutes(dhuhr_minutes)
{
  method_params[MWL]     = MethodConfig(18.0, true,  0.0, false, 17.0); // MWL
  method_params[ISNA]    = MethodConfig(15.0, true,  0.0, false, 15.0); // ISNA
  method_params[Egypt]   = MethodConfig(19.5, true,  0.0, false, 17.5); // Egypt
  method_params[Makkah]  = MethodConfig(18.5, true,  0.0, true,  90.0); // Makkah
  method_params[Karachi] = MethodConfig(18.0, true,  0.0, false, 18.0); // Karachi
  method_params[Jafari]  = MethodConfig(16.0, false, 4.0, false, 14.0); // Jafari
  method_params[Custom]  = MethodConfig(18.0, true,  0.0, false, 17.0); // Custom
}

/*
  MethodCobfig Parameters:
  double fajr_angle;
  bool   maghrib_is_minutes;
  double maghrib_value;   // angle or minutes
  bool   isha_is_minutes;
  double isha_value;    // angle or minutes
*/



/* return prayer times for a given date */
void PrayerTimes::get_prayer_times(int year, int month, int day, double _latitude, double _longitude, double _timezone, double times[])
{
  latitude = _latitude;
  longitude = _longitude;
  timezone = _timezone;
  julian_date = get_julian_date(year, month, day) - longitude / (double) (15 * 24);
  compute_day_times(times);
  tune_times(times);
}

/* return prayer times for a given date */
void PrayerTimes::get_prayer_times(time_t date, double latitude, double longitude, double timezone, double times[])
{
  tm* t = localtime(&date);
  get_prayer_times(1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, latitude, longitude, timezone, times);
}

/* return prayer times for the day after a given date */
void PrayerTimes::get_prayer_times_tomorrow(time_t date, double latitude, double longitude, double timezone, double timesTomorrow[])
{
  time_t numberOfSecondPerDay = 86400;
  //tm* t = localtime(&date);
  tm* t = localtime(&date + numberOfSecondPerDay);
  get_prayer_times(1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, latitude, longitude, timezone, timesTomorrow);
}

/* set the calculation method  */
void PrayerTimes::set_calc_method(CalculationMethod method_id)
{
  calc_method = method_id;
}

/* get hours and minutes parts of a float time */
void PrayerTimes::get_float_time_parts(double time, int& hours, int& minutes)
{
  time = fix_hour(time + 0.5 / 60);   // add 0.5 minutes to round
  hours = floor(time);
  minutes = floor((time - hours) * 60);
}


/* set the juristic method for Asr */
void PrayerTimes::set_asr_method(JuristicMethod method_id)
{
  asr_juristic = method_id;
}

/* set adjusting method for higher latitudes */
void PrayerTimes::set_high_lats_adjust_method(AdjustingMethod method_id)
{
  adjust_high_lats = method_id;
}

/* set the angle for calculating Fajr */
void PrayerTimes::set_fajr_angle(double angle)
{
  method_params[Custom].fajr_angle = angle;
  calc_method = Custom;
}

/* set the angle for calculating Maghrib */
void PrayerTimes::set_maghrib_angle(double angle)
{
  method_params[Custom].maghrib_is_minutes = false;
  method_params[Custom].maghrib_value = angle;
  calc_method = Custom;
}

/* set the angle for calculating Isha */
void PrayerTimes::set_isha_angle(double angle)
{
  method_params[Custom].isha_is_minutes = false;
  method_params[Custom].isha_value = angle;
  calc_method = Custom;
}

/* set the minutes after mid-day for calculating Dhuhr */
void PrayerTimes::set_dhuhr_minutes(double minutes)
{
  dhuhr_minutes = minutes;
}

/* set the minutes after Sunset for calculating Maghrib */
void PrayerTimes::set_maghrib_minutes(double minutes)
{
  method_params[Custom].maghrib_is_minutes = true;
  method_params[Custom].maghrib_value = minutes;
  calc_method = Custom;
}

/* set the minutes after Maghrib for calculating Isha */
void PrayerTimes::set_isha_minutes(double minutes)
{
  method_params[Custom].isha_is_minutes = true;
  method_params[Custom].isha_value = minutes;
  calc_method = Custom;
}



///* convert float hours to 24h format */
//String PrayerTimes::float_time_to_time24(double time)
//{
//  if ((std::isnan(time)))
//    return String();
//  int hours, minutes;
//  get_float_time_parts(time, hours, minutes);
//  return two_digits_format(hours) + ':' + two_digits_format(minutes);
//}



/* convert float hours to 24h format */
const char *PrayerTimes::float_time_to_time24(double time)
{
  if ((std::isnan(time)))
    return nullptr; //return NULL
  int hours, minutes;
  get_float_time_parts(time, hours, minutes);
  static char time24[6];
  const char* ptr = two_digits_format(hours);
  strlcpy(time24, ptr, sizeof(time24));
  strcat(time24, ":");
  ptr = two_digits_format(minutes);
  strcat(time24, ptr);
  return time24;
}

/* convert float hours to 12h format */
const char* PrayerTimes::float_time_to_time12(double time, bool no_suffix)
{
  if ((std::isnan(time)))
    return nullptr; // or return NULL;
  int hours, minutes;
  get_float_time_parts(time, hours, minutes);
  const char* suffix = hours >= 12 ? " PM" : " AM";
  hours = (hours + 12 - 1) % 12 + 1;
  //const char* ptrMinutes = two_digits_format(minutes);
  const char* ptrHours = two_digits_format(hours);
  const char* ptrMinutes = two_digits_format(minutes);
  static char time12[10];
  strlcpy(time12, ptrHours, sizeof(time12));
  strcat(time12, ":");
  strcat(time12, ptrMinutes);

  if (no_suffix) {
    return time12;
  }
  else {
    strcat(time12, suffix);
    return time12;
  }
  //return int_to_string(hours) + ':' + (String)ptrMinutes + (no_suffix ? "" : suffix);
}

/* convert float hours to 12h format with no suffix */
const char* PrayerTimes::float_time_to_time12ns(double time)
{
  return float_time_to_time12(time, true);
}




/* ---------------------- Time-Zone Functions ----------------------- */

/* compute local time-zone for a specific date */
double PrayerTimes::get_effective_timezone(time_t local_time)
{
  tm* tmp = localtime(&local_time);
  tmp->tm_isdst = 0;
  time_t local = mktime(tmp);
  tmp = gmtime(&local_time);
  tmp->tm_isdst = 0;
  time_t gmt = mktime(tmp);
  return (local - gmt) / 3600.0;
}

/* compute local time-zone for a specific date */
double PrayerTimes::get_effective_timezone(int year, int month, int day)
{
  tm date = { 0 };
  date.tm_year = year - 1900;
  date.tm_mon = month - 1;
  date.tm_mday = day;
  date.tm_isdst = -1;   // determine it yourself from system
  time_t local = mktime(&date);   // seconds since midnight Jan 1, 1970
  return get_effective_timezone(local);
}



/* ---------------------- Misc Functions ----------------------- */



const char* PrayerTimes::int_to_string(int num)
{
  static char tmp[16];
  tmp[0] = '\0';
  sprintf(tmp, "%02d", num);
  return tmp;
}


/* add a leading 0 if necessary */
const char* PrayerTimes::two_digits_format(int num)
{
  static char tmp[16];
  tmp[0] = '\0';
  sprintf(tmp, "%02d", num);
  return tmp;
}




/* ---------------------- Julian Date Functions ----------------------- */

/* calculate julian date from a calendar date */
double PrayerTimes::get_julian_date(int year, int month, int day)
{
  if (month <= 2)
  {
    year -= 1;
    month += 12;
  }

  double a = floor(year / 100.0);
  double b = 2 - a + floor(a / 4.0);

  return floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + day + b - 1524.5;
}

/* convert a calendar date to julian date (second method) */
double PrayerTimes::calc_julian_date(int year, int month, int day)
{
  double j1970 = 2440588.0;
  tm date = { 0 };
  date.tm_year = year - 1900;
  date.tm_mon = month - 1;
  date.tm_mday = day;
  date.tm_isdst = -1;   // determine it yourself from system
  time_t ms = mktime(&date);    // seconds since midnight Jan 1, 1970
  double days = floor(ms / (double) (60 * 60 * 24));
  return j1970 + days - 0.5;
}




/* ---------------------- Compute Prayer Times ----------------------- */

// array parameters must be at least of size TimesCount

/* compute prayer times at given julian date */
void PrayerTimes::compute_times(double times[])
{
  day_portion(times);

  times[Fajr]    = compute_time(180.0 - method_params[calc_method].fajr_angle, times[Fajr]);
  times[Sunrise] = compute_time(180.0 - 0.833, times[Sunrise]);
  times[Dhuhr]   = compute_mid_day(times[Dhuhr]);
  times[Asr]     = compute_asr(1 + asr_juristic, times[Asr]);
  times[Sunset]  = compute_time(0.833, times[Sunset]);
  times[Maghrib] = compute_time(method_params[calc_method].maghrib_value, times[Maghrib]);
  times[Isha]    = compute_time(method_params[calc_method].isha_value, times[Isha]);
}

/* compute prayer times at given julian date */
void PrayerTimes::compute_day_times(double times[])
{
  double default_times[] = { 5, 6, 12, 13, 18, 18, 18 };    // default times
  for (int i = 0; i < TimesCount; ++i)
    times[i] = default_times[i];

  for (int i = 0; i < NUM_ITERATIONS; ++i)
    compute_times(times);

  adjust_times(times);
}

/* adjust times in a prayer time array */
void PrayerTimes::adjust_times(double times[])
{
  for (int i = 0; i < TimesCount; ++i)
    times[i] += timezone - longitude / 15.0;
  times[Dhuhr] += dhuhr_minutes / 60.0;   // Dhuhr
  if (method_params[calc_method].maghrib_is_minutes)    // Maghrib
    times[Maghrib] = times[Sunset] + method_params[calc_method].maghrib_value / 60.0;
  if (method_params[calc_method].isha_is_minutes)   // Isha
    times[Isha] = times[Maghrib] + method_params[calc_method].isha_value / 60.0;

  if (adjust_high_lats != None)
    adjust_high_lat_times(times);
}

/* convert hours to day portions  */
void PrayerTimes::day_portion(double times[])
{
  for (int i = 0; i < TimesCount; ++i)
    times[i] /= 24.0;
}

/* adjust Fajr, Isha and Maghrib for locations in higher latitudes */
void PrayerTimes::adjust_high_lat_times(double times[])
{
  double night_time = time_diff(times[Sunset], times[Sunrise]);   // sunset to sunrise

  // Adjust Fajr
  double fajr_diff = night_portion(method_params[calc_method].fajr_angle) * night_time;
  if ((std::isnan(times[Fajr])) || time_diff(times[Fajr], times[Sunrise]) > fajr_diff)
    times[Fajr] = times[Sunrise] - fajr_diff;

  // Adjust Isha
  double isha_angle = method_params[calc_method].isha_is_minutes ? 18.0 : method_params[calc_method].isha_value;
  double isha_diff = night_portion(isha_angle) * night_time;
  if ((std::isnan(times[Isha])) || time_diff(times[Sunset], times[Isha]) > isha_diff)
    times[Isha] = times[Sunset] + isha_diff;

  // Adjust Maghrib
  double maghrib_angle = method_params[calc_method].maghrib_is_minutes ? 4.0 : method_params[calc_method].maghrib_value;
  double maghrib_diff = night_portion(maghrib_angle) * night_time;
  if ((std::isnan(times[Maghrib])) || time_diff(times[Sunset], times[Maghrib]) > maghrib_diff)
    times[Maghrib] = times[Sunset] + maghrib_diff;
}

/* compute the difference between two times  */
double PrayerTimes::time_diff(double time1, double time2)
{
  return fix_hour(time2 - time1);
}

/* the night portion used for adjusting times in higher latitudes */
double PrayerTimes::night_portion(double angle)
{
  switch (adjust_high_lats)
  {
    case AngleBased:
      return angle / 60.0;
    case MidNight:
      return 1.0 / 2.0;
    case OneSeventh:
      return 1.0 / 7.0;
    default:
      // Just to return something!
      // In original library nothing was returned
      // Maybe I should throw an exception
      // It must be impossible to reach here
      return 0;
  }
}

/* set offsets settings */
void PrayerTimes::tune (double timeOffsets[]) {
  for (unsigned int i = 0; i < TimesCount; ++i) {
    offset[i] = timeOffsets[i];
  }
}


/* apply offsets to the times */
double PrayerTimes::tune_times (double times[]) {
  for (unsigned int i = 0; i < TimesCount; ++i) {
    times[i] += offset[i] / 60;
  }
  return *times;
}



/* ---------------------- Calculation Functions ----------------------- */

/* References: */
/* http://www.ummah.net/astronomy/saltime   */
/* http://aa.usno.navy.mil/faq/docs/SunApprox.html */

/* compute time for a given angle G */
double PrayerTimes::compute_time(double g, double t)
{
  double d = sun_declination(julian_date + t);
  double z = compute_mid_day(t);
  double v = 1.0 / 15.0 * darccos((-dsin(g) - dsin(d) * dsin(latitude)) / (dcos(d) * dcos(latitude)));
  return z + (g > 90.0 ? - v :  v);
}

/* compute mid-day (Dhuhr, Zawal) time */
double PrayerTimes::compute_mid_day(double _t)
{
  double t = equation_of_time(julian_date + _t);
  double z = fix_hour(12 - t);
  return z;
}

/* compute the time of Asr */
double PrayerTimes::compute_asr(int step, double t)  // Shafii: step=1, Hanafi: step=2
{
  double d = sun_declination(julian_date + t);
  double g = -darccot(step + dtan(fabs(latitude - d)));
  return compute_time(g, t);
}

/* compute declination angle of sun */
double PrayerTimes::sun_declination(double jd)
{
  return sun_position(jd).first;
}

/* compute equation of time */
double PrayerTimes::equation_of_time(double jd)
{
  return sun_position(jd).second;
}

/* compute declination angle of sun and equation of time */
DoublePair PrayerTimes::sun_position(double jd)
{
  double d = jd - 2451545.0;
  double g = fix_angle(357.529 + 0.98560028 * d);
  double q = fix_angle(280.459 + 0.98564736 * d);
  double l = fix_angle(q + 1.915 * dsin(g) + 0.020 * dsin(2 * g));

  // double r = 1.00014 - 0.01671 * dcos(g) - 0.00014 * dcos(2 * g);
  double e = 23.439 - 0.00000036 * d;

  double dd = darcsin(dsin(e) * dsin(l));
  double ra = darctan2(dcos(e) * dsin(l), dcos(l)) / 15.0;
  ra = fix_hour(ra);
  double eq_t = q / 15.0 - ra;

  return DoublePair(dd, eq_t);
}

/* ---------------------- Trigonometric Functions ----------------------- */

/* range reduce hours to 0..23 */
double PrayerTimes::fix_hour(double a)
{
  a = a - 24.0 * floor(a / 24.0);
  a = a < 0.0 ? a + 24.0 : a;
  return a;
}

/* degree sin */
double PrayerTimes::dsin(double d)
{
  return sin(deg2rad(d));
}

/* degree cos */
double PrayerTimes::dcos(double d)
{
  return cos(deg2rad(d));
}

/* degree tan */
double PrayerTimes::dtan(double d)
{
  return tan(deg2rad(d));
}

/* degree arcsin */
double PrayerTimes::darcsin(double x)
{
  return rad2deg(asin(x));
}

/* degree arccos */
double PrayerTimes::darccos(double x)
{
  return rad2deg(acos(x));
}

/* degree arctan */
double PrayerTimes::darctan(double x)
{
  return rad2deg(atan(x));
}

/* degree arctan2 */
double PrayerTimes::darctan2(double y, double x)
{
  return rad2deg(atan2(y, x));
}

/* degree arccot */
double PrayerTimes::darccot(double x)
{
  return rad2deg(atan(1.0 / x));
}

/* degree to radian */
double PrayerTimes::deg2rad(double d)
{
  return d * M_PI / 180.0;
}

/* radian to degree */
double PrayerTimes::rad2deg(double r)
{
  return r * 180.0 / M_PI;
}

/* range reduce angle in degrees. */
double PrayerTimes::fix_angle(double a)
{
  a = a - 360.0 * floor(a / 360.0);
  a = a < 0.0 ? a + 360.0 : a;
  return a;
}






/* --------------------- Technical Settings -------------------- */

const int PrayerTimes::NUM_ITERATIONS = 1;    // number of iterations needed to compute times






