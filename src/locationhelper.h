#ifndef locationhelper_h
#define locationhelper_h

#include <stdint.h> // for uint32_t


typedef struct
{
  char province[32] = "JAWA BARAT";
  char regency[64] = "KOTA BEKASI";
  char district[64] = "BEKASI SELATAN";
  // int8_t timezone = 70;
  char timezonestring[32] = "WIB-7";
  double latitude = -6.263718;
  double longitude = 106.981958;
} strConfigLocation;
extern strConfigLocation _configLocation;



#endif // locationhelper_h