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

#ifndef _SHOLATLED_CONFIG_H
#define _SHOLATLED_CONFIG_H

// #include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <Hash.h>
#include <Ticker.h>
#include <DNSServer.h>
#include <ESP8266SSDP.h>
#include <ESP8266NetBIOS.h>
#include <ESPAsyncTCP.h>
#include <ESP8266SSDP.h>
#include <time.h>
// extern "C" {
// #include "sntp.h"
// }
#elif defined ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
#include "SPIFFS.h"
#include <ESP32Ticker.h>
#include "time.h"
#include "AsyncTCP.h"
#endif

#include <ESPAsyncWebServer.h>

#include <TimeLib.h>
#include "sholat.h"
#include "hub08.h"
#include "mqtt.h"
#include <Wire.h>
#include <RtcDS3231.h>      //RTC library
#include <StreamString.h>
#include "AsyncJson.h"
#include <ArduinoJson.h>

#include <ArduinoOTA.h>
#include <SPIFFSEditor.h>


#include "progmemmatrix.h"

// "PingAlive.h" Test library for https://github.com/esp8266/Arduino/issues/2330
#include "PingAlive.h"


/* Test for ESP platform */
#if defined(ARDUINO_ARCH_ESP8266)
#include <EEPROM.h>
//#include "spi_flash.h"
#else
#error "for ESP8266 boards only"
#endif

//#ifndef ESP8266
//#error "This library only supports boards with the ESP8266 MCU."
//#endif

extern "C" {
#include "sntp.h"
}

using namespace std;

#ifdef ESP8266
//extern "C" {
//#include "user_interface.h"
//#include "sntp.h"
//}
#define USE_OPTIMIZE_PRINTF
#include <functional>
using namespace std;
using namespace placeholders;
#endif

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif


#define PRINTPORT Serial
#define DEBUGPORT Serial

#define RELEASE

#define PROGMEM_T __attribute__((section(".irom.text.template")))
#define PRINT(fmt, ...) \
  { static const char pfmt[] PROGMEM_T = fmt; PRINTPORT.printf_P(pfmt, ## __VA_ARGS__); }

#ifndef RELEASE
#define DEBUGLOG(fmt, ...) \
  { static const char pfmt[] PROGMEM_T = fmt; DEBUGPORT.printf_P(pfmt, ## __VA_ARGS__); }
#else
#define DEBUGLOG(...)
#endif


//#define SET_ANALOG_FREQUENCY

#define DESCRIPTION_XML_FILE "/description.xml"

// Comment/uncomment the board you want to flash onto.
//#define ARDUINO
#define NODEMCU

#define PANEL_COLUMN  1
#define PANEL_ROW     1

#define WIDTH       (PANEL_COLUMN * 64)
#define HEIGHT      (PANEL_ROW * 16)

//#define WIDTH       64
//#define HEIGHT      16

#define DISP_REFF   WIDTH - 64
//#define DISP_REFF   0

#ifdef NODEMCU // for nodeMCU
#include <pins_arduino.h>
#endif

#define USE_IO_EXPANDER

#define IO_EXPANDER_ADDRESS 0x20



//The default pins are defined in variants/nodemcu/pins_arduino.h as SDA=4 and SCL=5
//but those are not pins number but GPIO number, so since the pins are D1=5=SCL and D2=4=SDA.
//Anyway, you can also choose the pins yourself using the I2C constructor Wire.begin(int sda, int scl);

/*
  yellow: CLK
  green: STB
  blue: R1
  grey: OE
  white: D
  black: C
  brown: B
  red: A
*/


#define ESP_PIN_0 0 //D3
#define ESP_PIN_1 1 //Tx
#define ESP_PIN_2 2 //D4 -> Led on NodeMcu
#define ESP_PIN_3 3 //D9(Rx)
#define ESP_PIN_4 4 //D2
#define ESP_PIN_5 5 //D1
#define ESP_PIN_9 9 //S2
#define ESP_PIN_10 10 //S3
#define ESP_PIN_12 12 //D6
#define ESP_PIN_13 13 //D7
#define ESP_PIN_14 14 //D5
#define ESP_PIN_15 15 //D8
#define ESP_PIN_16 16 //D0 -> Led on ESP8266

#define IO_EXPANDER_PIN_0 0
#define IO_EXPANDER_PIN_1 1
#define IO_EXPANDER_PIN_2 2
#define IO_EXPANDER_PIN_3 3
#define IO_EXPANDER_PIN_4 4
#define IO_EXPANDER_PIN_5 5
#define IO_EXPANDER_PIN_6 6
#define IO_EXPANDER_PIN_7 7

#ifdef USE_IO_EXPANDER

#define A IO_EXPANDER_PIN_7
#define B IO_EXPANDER_PIN_6
#define C IO_EXPANDER_PIN_5
#define D IO_EXPANDER_PIN_4
#define OE ESP_PIN_2 //D4 -> Led on NodeMcu
#define R1 ESP_PIN_12 //D6
#define STB ESP_PIN_15 //D8
#define CLK ESP_PIN_13 //D7
#define SDA ESP_PIN_0 //D3
#define SCL ESP_PIN_4 //D2

#define BUZZER ESP_PIN_14 //D5
#define RTC_SQW_PIN IO_EXPANDER_PIN_0
#define ENCODER_SW_PIN IO_EXPANDER_PIN_1
#define ENCODER_CLK_PIN IO_EXPANDER_PIN_2
#define ENCODER_DT_PIN IO_EXPANDER_PIN_3
#define MCU_INTERRUPT_PIN ESP_PIN_5 //D1
#define LED_1 ESP_PIN_16 //D0 -> Led on ESP8266

#else

#define A 16 //D0
#define B 5 //D1
#define C 4 //D2
#define D 0 //D3
#define OE 2 //D4
#define R1 14 //D5
#define STB 15 //D8
#define CLK 3 //D9(Rx)
#define SDA 13 //D7
#define SCL 12 //D6
#define BUZZER 10 //1 //Tx

#endif





extern const uint16_t ANALOGWRITE_BASE_FREQUENCY;
extern uint16_t ANALOGWRITE_OPERATING_FREQUENCY;

extern char sholatTimeYesterdayArray[TimesCount][6];
extern char sholatTimeArray[TimesCount][6];
extern char sholatTimeTomorrowArray[TimesCount][6];

//extern char CURRENTTIMENAME[20];
//extern char NEXTTIMENAME[20];

extern char bufHOUR[2];
extern char bufMINUTE[2];
extern char bufSECOND[2];
extern char bufSECONDMATRIX[2];

extern int CURRENTTIMEID, NEXTTIMEID;
extern int weekday_old;
//extern int lenCURRENTTIMENAME, lenNEXTTIMENAME;
extern int HOUR;
extern int MINUTE;
extern int SECOND;
extern int X;



extern unsigned int scrollSpeed;



extern boolean PROCESS_SHOLAT;
extern boolean NTP_OK;
extern boolean syncNtpEventTriggered; // True if a time even has been triggered
extern boolean syncRtcEventTriggered; // True if a time even has been triggered
extern boolean syncSuccessByNtp;
extern boolean syncSuccessByRtc;



extern boolean stateSwitch;
extern boolean FORCE_UPDATE_TIME;
extern boolean tick500ms;
extern boolean tick1000ms;
extern boolean state250ms;
extern boolean state500ms;
extern boolean state1000ms;

extern char bufferStr[]; // must be big enough for longest string and the terminating null

extern const char dayNameStr0[];
extern const char dayNameStr1[];
extern const char dayNameStr2[];
extern const char dayNameStr3[];
extern const char dayNameStr4[];
extern const char dayNameStr5[];
extern const char dayNameStr6[];
extern const char dayNameStr7[];

//extern const char pgmTxtInfoDynamic[];
#define dt_SHORT_STR_LEN 3 // the length of short strings

extern char bufferHari[]; // must be big enough for longest string and the terminating null

extern const char hariStr0[];
extern const char hariStr1[];
extern const char hariStr2[];
extern const char hariStr3[];
extern const char hariStr4[];
extern const char hariStr5[];
extern const char hariStr6[];
extern const char hariStr7[];

extern const PROGMEM char *const PROGMEM hariNames_P[];

extern const char hariShortNames_P[];

extern PrayerTimes sholat;




// -------------------------------------------------------------------
// Load and save the EmonESP config.
//
// This initial implementation saves the config to the EEPROM area of flash
// -------------------------------------------------------------------

// Global config varables

// TIME variables
//extern time_t utc_time, local_time;
extern time_t _lastSyncd;      ///< Stored time of last successful sync
extern time_t _firstSync;      ///< Stored time of first successful sync after boot
extern unsigned long _millisFirstSync;
extern time_t _secondsFirstSync;
extern time_t _lastBoot;
extern time_t utcTime, localTime;
extern time_t utc_time();
extern time_t local_time();
extern time_t local_time(time_t t);
extern time_t getUnsyncTime();
//int _shortInterval;         ///< Interval to set periodic time sync until first synchronization.
//int _longInterval;          ///< Interval to set periodic time sync
extern bool setInterval (int interval);
extern bool setInterval (int shortInterval, int longInterval);
extern int getInterval();
extern int getShortInterval();

// Page Mode
typedef enum timeSource
{
  TIMESOURCE_NOT_AVAILABLE,
  TIMESOURCE_NTP,
  TIMESOURCE_RTC
} TIMESOURCE;

extern TIMESOURCE _timeSource;

// Config IDs
typedef enum enumConfigID
{
  WIFI,
  TIME,
  SHOLAT,
  LEDMATRIX,
  MQTT,

  CONFIGSCOUNT
} CONFIGID;



typedef struct {
  char hostname[32] = "ESP_XXXX";
  char ssid[32];
  char password[32];
  bool dhcp = true;
  char static_ip[16] = "192.168.10.15";
  char netmask[16] = "255.255.255.0";
  char gateway[16] = "192.168.10.1";
  char dns0[16] = "192.168.10.1";
  char dns1[16] = "8.8.8.8";
} strConfig;

extern strConfig _config; // General and WiFi configuration

typedef struct
{
  const char* ssid = _config.hostname; // ChipID is appended to this name
  char password[10] = ""; // NULL means no password
  bool APenable = false; // AP disabled by default
} strApConfig;

extern strApConfig _configAP; // Static AP config settings

typedef struct {
  char city[48] = "KOTA BEKASI";
  int8_t timezone = 70;
  double latitude = -6.263718;
  double longitude = 106.981958;
} strConfigLocation;

extern strConfigLocation _configLocation;

typedef struct {
  // int8_t timezone = _configLocation.timezone;
  bool dst = false;
  bool enablertc = true;
  uint32_t syncinterval = 600;
  bool enablentp = true;
  char ntpserver_0[48] = "0.id.pool.ntp.org";
  char ntpserver_1[48] = "0.asia.pool.ntp.org";
  char ntpserver_2[48] = "192.168.10.1";  
} strConfigTime;

extern strConfigTime _configTime;

typedef enum _enumTimezone
{
  WIB,
  WITA,
  WIT,

  timezoneCount
} enumTimezone;


typedef struct {
  bool auth;
  char wwwUsername[32];
  char wwwPassword[32];
} strHTTPAuth;

extern strHTTPAuth _httpAuth;

// Sholat settings
typedef struct {
  // char location[48];
  // const char* location = "KOTA BEKASI";
  // int8_t timezone = _configLocation.timezone;
  // double latitude = _configLocation.latitude;
  // double longitude = _configLocation.longitude;  
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


// Page Mode
typedef enum OperatingMode
{
  Normal,
  Config,
  Edit,
  OperatingModeCount

} OPERATING_MODE;

// Page Mode
typedef enum PageMode
{
  Automatic,
  Manual,
  PageModeCount

} PAGE_MODE;

// Led Matrix settings
typedef struct {
  OperatingMode operatingmode = Normal;
  PageMode pagemode = Automatic;
  uint8_t pagemode0 = 0;
  uint8_t pagemode1 = 0;
  uint8_t pagemode2 = 0;
  bool scrollrow_0 = false;
  bool scrollrow_1 = false;
  uint16_t scrollspeed = 10;
  uint16_t brightness = 50;
  uint8_t adzanwaittime = 6;
  uint8_t iqamahwaittime = 5;

} ledMatrixSettings;

extern ledMatrixSettings _ledMatrixSettings;
extern bool blinkColon;

// Page Mode
typedef enum PageTitleMode0
{
  jam_dan_tanggal,
  jam_menit_menuju_sholat,
  waktu_sholat_saat_ini_type_0,
  jam_saja,
  daya_listrik,
  quran_hadist,
  temperature,
  sandbox,
  waktu_sholat_saat_ini_type_1,
  jam_saja_type_1,
  sanbox_2,
  iqamah_time,
  jam_dan_tanggal_type_2,

  PageTitleMode0Count

} PAGETITLE_MODE_0;

typedef enum PageTitleMode1
{
  device_name,
  ssid,
  ip_address,
  gateway,
  netmask,
  mac_address,
  dns_address,
  last_sync,
  current_time,
  current_date,
  uptime,
  lastboot,
  freeheap,

  PageTitleMode1Count

} PAGETITLE_MODE_1;

typedef enum PageTitleMode2
{
  set_timezone,
  set_coordinate,
  set_date,
  set_time,
  PageTitleMode2Count
} PAGETITLE_MODE_2;

typedef enum {
  RTC_TIME_VALID,
  RTC_LOST_CONFIDENT,
  CLOCK_NOT_RUNNING
} RTCStatus;


// Led Matrix Settings
//extern byte currentPage;
//extern byte currentPage_old;
extern byte currentPageMode0;
extern byte currentPageMode0_old;
extern byte currentPageMode1;
extern byte currentPageMode1_old;
extern byte currentPageMode2;
extern byte currentPageMode2_old;
//extern byte OPERATING_MODE;
extern byte MODE;
extern byte MODE_old;
extern OperatingMode OPERATING_MODE_old;


// -------------------------------------------------------------------
// Reset the config back to defaults
// -------------------------------------------------------------------
extern void config_reset();
extern void ResetEEPROM();

// -------------------------------------------------------------------
// OTHERS
// -------------------------------------------------------------------

extern HUB08 matrix;

//extern RtcDS3231<TwoWire>;
extern RtcDS3231<TwoWire> Rtc;

extern time_t lastSyncRTC;

extern uint16_t wText, hText;

//float watt;
extern char bufWatt[10];
//float voltage;
extern char bufVoltage[10];

extern char bufTempSensor[10];

extern char bufRtcTemp[10];

extern byte utf8ascii(byte ascii);

extern void utf8ascii(char* s);

extern void printDateTime(const RtcDateTime & dt);

extern byte hourMinuteSecondToNextSholatTime[6];

extern time_t secondsLeftToNextSholatTime[6];

extern time_t timeRtcNow;

extern boolean RTC_OK;
extern boolean RTC_OK_OLD;

extern bool wifiFirstConnected;

void process_sholat();

extern const char *const dayNamesStr_P[];

extern const char Page_Restart[];
extern const char Page_WaitAndReload[];
extern const char modelName[];
extern const char modelNumber[];
extern const char pgm_txt_manual[];
extern const char pgm_txt_hapelaptop[];
extern const char pgm_txt_timeis[];
extern const char pgm_txt_syncChoice[];
extern const char pgm_txt_Automatic[];
extern const char pgm_txt_Manual[];
extern const char pgm_txt_gotyourtextmessage[];
extern const char pgm_txt_gotyourbinarymessage[];
extern const char pgm_txt_serverloseconnection[];
extern const char pgm_txt_matrixConfig[];
extern const char pgm_txt_sholatDynamic[];
extern const char pgm_txt_sholatStatic[];
extern const char pgm_txt_infoStatic[];

char *dayNameStr(uint8_t day);

char* getDateStr (time_t moment);
char* GetRtcStatusString();
char* getDateTimeString (time_t moment);
char* getTimeStr (time_t moment);
char* getLastSyncStr();
char* getNextSyncStr();
char* getLastBootStr();
char* getDriftStr();
char* getUptimeStr();

char* sholatNameStr(uint8_t id);


time_t GetRtcTime();

void digitalClockDisplay();
void digitalClockDisplay(time_t t);

char *hariStr(uint8_t day);
char *hariShortStr(uint8_t day);

time_t tmConvert_t(int YYYY, byte MM, byte DD, byte hh, byte mm, byte ss);
void printDigits(int digits);

byte RtcStatus();










#endif // _SHOLATLED_CONFIG_H




