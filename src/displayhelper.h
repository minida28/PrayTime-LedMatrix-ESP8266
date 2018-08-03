#ifndef displayhelper_h
#define displayhelper_h

#include "pinouthelper.h"
#include "hub08.h"
#include "buzzer.h"
#include "ioexpanderhelper.h"
#include "locationhelper.h"

#include "timehelper.h"
#include "sholathelper.h"
#include <ESP8266WiFi.h>
#include "encoderhelper.h"
#include "mqtt.h"
#include <Ticker.h>

#include "font.h"

#include <Fonts/TomThumb.h>
#include <Fonts/FreeSerifBold9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/Org_01.h>




#define PANEL_COLUMN  1
#define PANEL_ROW     1

#define WIDTH       (PANEL_COLUMN * 64)
#define HEIGHT      (PANEL_ROW * 16)

//#define WIDTH       64
//#define HEIGHT      16

#define DISP_REFF   WIDTH - 64


extern HUB08 matrix;

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
  set_province,
  set_regency,
  set_district,
  set_date,
  set_time,
  PageTitleMode2Count
} PAGETITLE_MODE_2;

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

extern bool enterEditModeFromShortcut;

extern Ticker tickerRefreshDisplay;


extern uint16_t wText, hText;

extern unsigned int scrollSpeed;

extern int X;

extern char bufSECONDMATRIX[3];


extern bool enterEditModeFromShortcut;
extern bool blinkColon;


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

void AlarmTrigger();
uint8_t PageAutomaticMode();

void refreshDisplay(int times);

void DisplaySetup();
void DisplayLoop();

void process_runningled_page();

#endif