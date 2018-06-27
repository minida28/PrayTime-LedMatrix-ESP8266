// Added by me
// #include "Arduino.h"

#include "tcpCleanup-snippet.h"


#include "config.h"
#include "SparkFunTMP102.h" //TMP102 I2C Temperature Sensor
#include "font.h"

#include "mqtt.h"
#include "asyncserver.h"
#include "buzzer.h"

#include <Fonts/TomThumb.h>
#include <Fonts/FreeSerifBold9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/Org_01.h>

#define SNTP_DEBUG (LWIP_DBG_ON | LWIP_DBG_LEVEL_WARNING)

//char h_digit_1_old[1], h_digit_2_old[1], m_digit_1_old[1], m_digit_2_old[1], s_digit_1_old[1], s_digit_2_old[1];

// static bool encoder0PosIncreasing;
// static bool encoder0PosDecreasing;
int encoder0Pos = 0;
bool enterEditModeFromShortcut = false;

volatile bool PCFInterruptFlag = false;

void ICACHE_RAM_ATTR PCFInterrupt()
{
  PCFInterruptFlag = true;
}

TMP102 TMP102(0x48); // Initialize sensor at I2C address 0x48
//AsyncMqttClient mqttClient;

//toggleSwitch toggleSwitch(interruptPin, 50, 50);

Ticker flipper250ms;
Ticker tickerSetHigh;

void refresh()
{
  matrix.scan(); //refresh a single line of the display
}

int pgm_lastIndexOf(uint8_t c, const char *p)
{
  int last_index = -1; // -1 indicates no match
  uint8_t b;
  for (int i = 0; true; i++)
  {
    b = pgm_read_byte(p++);
    if (b == c)
      last_index = i;
    else if (b == 0)
      break;
  }
  return last_index;
}

// displays at startup the Sketch running in the Arduino
void display_srcfile_details(void)
{
  const char *the_path = PSTR(__FILE__); // save RAM, use flash to hold __FILE__ instead

  int slash_loc = pgm_lastIndexOf('/', the_path); // index of last '/'
  if (slash_loc < 0)
    slash_loc = pgm_lastIndexOf('\\', the_path); // or last '\' (windows, ugh)

  int dot_loc = pgm_lastIndexOf('.', the_path); // index of last '.'
  if (dot_loc < 0)
    dot_loc = pgm_lastIndexOf(0, the_path); // if no dot, return end of string

  Serial.print(F("\nSketch name: "));

  for (int i = slash_loc + 1; i < dot_loc; i++)
  {
    uint8_t b = pgm_read_byte(&the_path[i]);
    if (b != 0)
      Serial.print((char)b);
    else
      break;
  }
  Serial.println();

  Serial.print(F("Compiled on: "));
  Serial.print(__DATE__);
  Serial.print(F(" at "));
  Serial.println(__TIME__);
  Serial.println();
}

//***** RTC *******//

int I2C_ClearBus()
{
#if defined(TWCR) && defined(TWEN)
  TWCR &= ~(_BV(TWEN)); //Disable the Atmel 2-Wire interface so we can control the SDA and SCL pins directly
#endif

  pinMode(SDA, INPUT_PULLUP); // Make SDA (data) and SCL (clock) pins Inputs with pullup.
  pinMode(SCL, INPUT_PULLUP);

  // Wait 2.5 secs, i.e. delay(2500). This is strictly only necessary on the first power
  // up of the DS3231 module to allow it to initialize properly,
  // but is also assists in reliable programming of FioV3 boards as it gives the
  // IDE a chance to start uploaded the program
  // before existing sketch confuses the IDE by sending Serial data.
  Serial.println(F("Delay 2.5 secs to allow DS3231 module to initialize properly"));
  delay(2500);
  boolean SCL_LOW = (digitalRead(SCL) == LOW); // Check is SCL is Low.
  if (SCL_LOW)
  {           //If it is held low Arduno cannot become the I2C master.
    return 1; //I2C bus error. Could not clear SCL clock line held low
  }

  boolean SDA_LOW = (digitalRead(SDA) == LOW); // vi. Check SDA input.
  int clockCount = 20;                         // > 2x9 clock

  while (SDA_LOW && (clockCount > 0))
  { //  vii. If SDA is Low,
    clockCount--;
    // Note: I2C bus is open collector so do NOT drive SCL or SDA high.
    pinMode(SCL, INPUT);        // release SCL pullup so that when made output it will be LOW
    pinMode(SCL, OUTPUT);       // then clock SCL Low
    delayMicroseconds(10);      //  for >5uS
    pinMode(SCL, INPUT);        // release SCL LOW
    pinMode(SCL, INPUT_PULLUP); // turn on pullup resistors again
    // do not force high as slave may be holding it low for clock stretching.
    delayMicroseconds(10); //  for >5uS
    // The >5uS is so that even the slowest I2C devices are handled.
    SCL_LOW = (digitalRead(SCL) == LOW); // Check if SCL is Low.
    int counter = 20;
    while (SCL_LOW && (counter > 0))
    { //  loop waiting for SCL to become High only wait 2sec.
      counter--;
      delay(100);
      SCL_LOW = (digitalRead(SCL) == LOW);
    }
    if (SCL_LOW)
    {           // still low after 2 sec error
      return 2; // I2C bus error. Could not clear. SCL clock line held low by slave clock stretch for >2sec
    }
    SDA_LOW = (digitalRead(SDA) == LOW); //   and check SDA input again and loop
  }
  if (SDA_LOW)
  {           // still low
    return 3; // I2C bus error. Could not clear. SDA data line held low
  }

  // else pull SDA line low for Start or Repeated Start
  pinMode(SDA, INPUT);  // remove pullup.
  pinMode(SDA, OUTPUT); // and then make it LOW i.e. send an I2C Start or Repeated start control.
  // When there is only one I2C master a Start or Repeat Start has the same function as a Stop and clears the bus.
  /// A Repeat Start is a Start occurring after a Start with no intervening Stop.
  delayMicroseconds(10);      // wait >5uS
  pinMode(SDA, INPUT);        // remove output low
  pinMode(SDA, INPUT_PULLUP); // and make SDA high i.e. send I2C STOP control.
  delayMicroseconds(10);      // x. wait >5uS
  pinMode(SDA, INPUT);        // and reset pins as tri-state inputs which is the default state on reset
  pinMode(SCL, INPUT);
  return 0; // all ok
}

time_t getNtpTimeSDK()
{
  syncNtpEventTriggered = true;

  digitalClockDisplay(localTime);
  Serial.print(F("> "));
  Serial.println();
  digitalClockDisplay(localTime);
  Serial.print(F("> "));
  Serial.println(F("Execute getNtpTimeSDK()"));

  digitalClockDisplay(localTime);
  Serial.print(F("> "));
  Serial.print(F("NTP server-0: "));
  Serial.println(sntp_getservername(0));

  digitalClockDisplay(localTime);
  Serial.print(F("> "));
  Serial.print(F("NTP server-1: "));
  Serial.println(sntp_getservername(1));

  digitalClockDisplay(localTime);
  Serial.print(F("> "));
  Serial.print(F("NTP server-2: "));
  Serial.println(sntp_getservername(2));

  digitalClockDisplay(localTime);
  Serial.print(F("> "));
  Serial.print(F("Timezone: "));
  Serial.println(sntp_get_timezone());

  //Serial.println((unsigned long)sntp_get_current_timestamp());
  //Serial.println((unsigned long)sntp_get_real_time(sntp_get_current_timestamp()));

  digitalClockDisplay(localTime);
  Serial.print(F("> "));
  Serial.println(F("Transmit NTP request"));

  unsigned long startMicros = micros();

  uint32_t current_stamp = sntp_get_current_timestamp();

  unsigned long endMicros = micros();

  digitalClockDisplay(localTime);
  Serial.print(F("> start: "));
  Serial.print(startMicros);
  Serial.print(F(", end: "));
  Serial.println(endMicros);

  digitalClockDisplay(localTime);
  Serial.print(F("> Transmit time: "));
  Serial.print(endMicros - startMicros);
  Serial.println(F(" us"));

  //Serial.println(current_stamp);

  if (current_stamp > 1514764800)
  {
    char str[50];

    sprintf(str, "Received timestamp: %d, %s", current_stamp, sntp_get_real_time(current_stamp));

    digitalClockDisplay(localTime);
    Serial.print(F("> "));
    Serial.print(str); //print the string to the serial port

    //str[0] = '\0';
    //sprintf(str, "%d", current_stamp, sntp_get_real_time(current_stamp));

    //char *ptr;
    //unsigned long real_stamp;
    //real_stamp = strtoul(str, &ptr, 10);
    //Serial.println(real_stamp);

    digitalClockDisplay(localTime);
    Serial.print(F("> "));
    Serial.println(F("Timestamp is valid, later than 1 January 2018 :-)"));

    // store as last sync timestamp
    _lastSyncd = current_stamp;
    return current_stamp;
  }
  else if (current_stamp > 0 && current_stamp < 1514764800)
  {
    digitalClockDisplay(localTime);
    Serial.print(F("> "));
    Serial.println(F("Timestamp is invalid, older than 1 January 2018 :-("));
    return 0;
  }
  else if (current_stamp == 0)
  {
    digitalClockDisplay(localTime);
    Serial.print(F("> "));
    Serial.println(F("No NTP Response :-("));
    return 0;
  }

  Serial.println(F("Other error"));
  return 0;
}

byte PrintTimeStatus()
{

  digitalClockDisplay();
  Serial.print(F("> "));
  Serial.print(F("Time status: "));

  byte currentTimeStatus = timeStatus();

  if (currentTimeStatus == timeNotSet)
  {
    Serial.println(F("TIME NOT SET"));
  }
  else if (currentTimeStatus == timeNeedsSync)
  {
    Serial.println(F("TIME NEED SYNC"));
  }
  else if (currentTimeStatus == timeSet)
  {
    Serial.println(F("TIME SET"));
  }
  return currentTimeStatus;
}

time_t get_time_from_rtc()
{
  syncRtcEventTriggered = true;

  digitalClockDisplay(localTime);
  Serial.print(F("> "));
  Serial.println();
  digitalClockDisplay(localTime);
  Serial.print(F("> "));
  Serial.println(F("Execute get_time_from_rtc()"));

  if (RtcStatus() == RTC_TIME_VALID)
  {

    syncSuccessByRtc = true;

    RtcDateTime dt = Rtc.GetDateTime();

    /*
      while (dt == Rtc.GetDateTime()) {
      // wait the RTC time to change
      }
      // save the new time
      dt = Rtc.GetDateTime();
    */

    time_t temp_local_time = dt + _configLocation.timezone / 10.0 * 3600;
    time_t t = dt.Epoch32Time();

    printDateTime(temp_local_time);
    Serial.print(F("> "));
    Serial.println(F("RTC time is valid"));

    printDateTime(temp_local_time);
    Serial.print(F("> "));
    Serial.print(F("RTC timestamp: "));
    Serial.print(t);
    Serial.print(F(", "));
    printDateTime(dt);
    Serial.println();

    _lastSyncd = t;

    return t;
  }
  printDateTime(localTime);
  Serial.print(F("> "));
  //Serial.println(F("RTC time is INVALID"));
  Serial.println(GetRtcStatusString());
  return 0; // return 0 if unable to get the time
}

time_t currentSholatTime = 0;
time_t nextSholatTime = 0;

void process_sholat_2nd_stage()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  time_t timestamp_now = 0;

  time_t s_tm = 0;

  timestamp_now = local_time();

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
      time_t timestamp_current_today;
      time_t timestamp_next_today;
      time_t timestamp_next_tomorrow;

      timestamp_current_today = sholat.timestampSholatTimesToday[tempCurrentID];

      timestamp_next_today = sholat.timestampSholatTimesToday[tempNextID];

      timestamp_next_tomorrow = sholat.timestampSholatTimesTomorrow[tempNextID];

      if (timestamp_current_today < timestamp_next_today)
      {
        if (timestamp_now <= timestamp_current_today && timestamp_now < timestamp_next_today)
        {
          CURRENTTIMEID = tempPreviousID;
          NEXTTIMEID = tempCurrentID;
          s_tm = timestamp_current_today;

          break;
        }
        else if (timestamp_now > timestamp_current_today && timestamp_now <= timestamp_next_today)
        {
          CURRENTTIMEID = tempCurrentID;
          NEXTTIMEID = tempNextID;
          s_tm = timestamp_next_today;

          break;
        }
      }
      else if (timestamp_current_today > timestamp_next_today)
      {
        if (timestamp_now >= timestamp_current_today && timestamp_now < timestamp_next_tomorrow)
        {
          CURRENTTIMEID = tempCurrentID;
          NEXTTIMEID = tempNextID;
          s_tm = timestamp_next_tomorrow;

          break;
        }
      }
    }
  } //end of for loop

  time_t timestamp_current_yesterday;
  time_t timestamp_current_today;
  time_t timestamp_next_today;
  time_t timestamp_next_tomorrow;

  timestamp_current_yesterday = sholat.timestampSholatTimesYesterday[CURRENTTIMEID];
  timestamp_current_today = sholat.timestampSholatTimesToday[CURRENTTIMEID];
  timestamp_next_today = sholat.timestampSholatTimesToday[NEXTTIMEID];
  timestamp_next_tomorrow = sholat.timestampSholatTimesTomorrow[NEXTTIMEID];

  if (NEXTTIMEID > CURRENTTIMEID)
  {
    currentSholatTime = timestamp_current_today;
    nextSholatTime = timestamp_next_today;
    //PRINT("%s %lu %lu\n", "Case 2a", currentSholatTime, nextSholatTime);
  }
  else if (NEXTTIMEID < CURRENTTIMEID)
  {
    if (isPM(localTime))
    {
      currentSholatTime = timestamp_current_today;
      nextSholatTime = timestamp_next_tomorrow;
      //PRINT("%s %lu %lu\n", "Case 2b", currentSholatTime, nextSholatTime);
    }
    if (isAM(localTime))
    {
      currentSholatTime = timestamp_current_yesterday;
      nextSholatTime = timestamp_next_today;
      //PRINT("%s %lu %lu\n", "Case 2c", currentSholatTime, nextSholatTime);
    }
  }

  //uint8_t lenCURRENTTIMENAME = strCURRENTTIMENAME.length();
  //char bufCURRENTTIMENAME[lenCURRENTTIMENAME + 1];

  //lenNEXTTIMENAME = NEXTTIMENAME.length();
  //char bufNEXTTIMENAME[lenNEXTTIMENAME + 1];

  //  uint8_t len1 = strlen(CURRENTTIMENAME);
  //  char bufCURRENTTIMENAME[len1 + 1];
  //
  //  uint8_t len2 = strlen(NEXTTIMENAME);
  //  char bufNEXTTIMENAME[len2 + 1];

  time_t timeDiff = s_tm - local_time();

  // uint16_t days;
  uint8_t hr;
  uint8_t mnt;
  uint8_t sec;

  /* // METHOD 1

    char *s;
    s = TimeToString(timeDiff, &hr, &mnt, &sec);

    //  DEBUGLOG(getDateTimeString(local_time()).c_str());
    //  DEBUGLOG("> ");
    //  DEBUGLOG(TimeName[CURRENTTIMEID]);
    //  DEBUGLOG("->");
    //  DEBUGLOG(TimeName[NEXTTIMEID]);
    //  DEBUGLOG(", ");
    //  DEBUGLOG("ts_Now: ");
    //  DEBUGLOG("%ld", local_time();
    //  DEBUGLOG(", ");
    //  DEBUGLOG("ts_Next: ");
    //  DEBUGLOG("%ld", s_tm);
    //  DEBUGLOG(", ");
    //  DEBUGLOG("diff: ");
    //  DEBUGLOG("%ld", timeDiff);
    //  DEBUGLOG(", ");
    //  DEBUGLOG("h,m,s: %d:%d:%d\n\r", hr, mnt, sec);
    //  //DEBUGLOG(s);
  */

  // METHOD 2
  //days = elapsedDays(timeDiff);
  hr = numberOfHours(timeDiff);
  mnt = numberOfMinutes(timeDiff);
  sec = numberOfSeconds(timeDiff);

  HOUR = hr;
  MINUTE = mnt;
  SECOND = sec;

  static int HOUR_old = 100;

  if (HOUR != HOUR_old)
  {
    HOUR_old = HOUR;
    dtostrf(HOUR, 1, 0, bufHOUR);
  }

  static int MINUTE_old = 100;

  if (MINUTE != MINUTE_old)
  {
    MINUTE_old = MINUTE;
    dtostrf(MINUTE, 1, 0, bufMINUTE);
  }

  static int SECOND_old = 100;

  if (SECOND != SECOND_old)
  {
    SECOND_old = SECOND;
    dtostrf(SECOND, 1, 0, bufSECOND);
    dtostrf(SECOND, 2, 0, bufSECONDMATRIX);
  }

  if (SECOND == 0)
  {
    digitalClockDisplay();
    Serial.print(F("> "));

    if (HOUR != 0 || MINUTE != 0)
    {
      if (HOUR != 0)
      {
        Serial.print(bufHOUR);
        Serial.print(F(" jam "));
      }
      Serial.print(bufMINUTE);
      Serial.print(F(" min menuju "));
      Serial.println(sholatNameStr(NEXTTIMEID));
    }
    //else if (HOUR == 0 && MINUTE == 0) {
    else if (HOUR == 0 && MINUTE == 0)
    {
      Serial.print(F("Waktu "));
      Serial.print(sholatNameStr(NEXTTIMEID));
      Serial.println(F(" telah masuk!"));
      Serial.println(F("Dirikanlah sholat tepat waktu."));
    }
  }
}

//http://forum.arduino.cc/index.php?topic=45293.msg328363#msg328363
// t is time in seconds = millis()/1000;
char *TimeToString(unsigned long t, long *hr, int *mnt, int *sec)
{
  static char str[12];
  unsigned long h = t / 3600;
  t = t % 3600;
  int m = t / 60;
  int s = t % 60;

  *hr = h;
  *mnt = m;
  *sec = s;
  //sprintf(str, "%04ld:%02d:%02d", h, m, s);
  sprintf(str, "%lu:%d:%d", h, m, s);
  return str;
}

void flip250ms()
{
  //  static boolean state250ms;
  state250ms = !state250ms;
}

void flip500ms()
{
  //  static boolean state500ms;
  state500ms = !state500ms;
}

void PreparePage(char *contents, const GFXfont *font, int16_t *x1, int16_t *y1)
{
  matrix.setFont(font);

  //char *str;
  //str = const_cast<char *>(contents.c_str());

  int16_t x1Temp, y1Temp;
  uint16_t w1Temp, h1Temp;

  //matrix.getTextBounds(str, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);
  matrix.getTextBounds(contents, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);
  *x1 = (matrix.width() - w1Temp) / 2;
  *y1 = (matrix.height() - h1Temp) / 2 + h1Temp - 1;
}

boolean slide_in_down_sec_digit_1(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
  //FONT
  //font = &FreeSerifBold9pt7b;
  //font = &UbuntuMono_B10pt8b;
  //font = &bold_led_board_7_regular4pt7b;
  //font = &RepetitionScrolling5pt8b;
  //font = &ChessType9pt7b;
  //font = &Org_01;
  //font = &FreeMonoBold12pt7b;
  //font = &FreeSansBold9pt7b;
  //font = &FreeSans9pt7b;

  matrix.setFont(font);

  int16_t x1 = x0;
  int16_t y1 = y0 - h + 1;

  //erase area and print next second
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);
  matrix.setCursor(x0, y0);
  matrix.print(chr);

  //copy to buffer
  uint16_t buf[w * h];
  matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

  //erase area again :)
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);

  static int16_t offset = 0;
  uint16_t index = 0;

  for (uint16_t yTemp = 0; yTemp < h; yTemp++)
  {
    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
    {
      if (yTemp >= h - offset)
      {
        index = w * yTemp + xTemp;
        matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
      }
      else
      {
        //matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
      }
    }
  }
  offset++;

  //reset offset and change to next animation
  if (offset > h)
  {
    offset = 0;
    return true;
  }
  else
  {
    return false;
  }
}

boolean slide_in_down_sec_digit_2(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
  //FONT
  //font = &FreeSerifBold9pt7b;
  //font = &UbuntuMono_B10pt8b;
  //font = &bold_led_board_7_regular4pt7b;
  //font = &RepetitionScrolling5pt8b;
  //font = &ChessType9pt7b;
  //font = &Org_01;
  //font = &FreeMonoBold12pt7b;
  //font = &FreeSansBold9pt7b;
  //font = &FreeSans9pt7b;

  matrix.setFont(font);

  int16_t x1 = x0;
  int16_t y1 = y0 - h + 1;

  //erase area and print next second
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);
  matrix.setCursor(x0, y0);
  matrix.print(chr);

  //copy to buffer
  uint16_t buf[w * h];
  matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

  //erase area again :)
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);

  static int16_t offset = 0;
  uint16_t index = 0;

  for (uint16_t yTemp = 0; yTemp < h; yTemp++)
  {
    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
    {
      if (yTemp >= h - offset)
      {
        index = w * yTemp + xTemp;
        matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
      }
      else
      {
        //matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
      }
    }
  }
  offset++;

  //reset offset and change to next animation
  if (offset > h)
  {
    offset = 0;
    return true;
  }
  else
  {
    return false;
  }
}

boolean slide_in_down_min_digit_1(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
  //FONT
  //font = &FreeSerifBold9pt7b;
  //font = &UbuntuMono_B10pt8b;
  //font = &bold_led_board_7_regular4pt7b;
  //font = &RepetitionScrolling5pt8b;
  //font = &ChessType9pt7b;
  //font = &Org_01;
  //font = &FreeMonoBold12pt7b;
  //font = &FreeSansBold9pt7b;
  //font = &FreeSans9pt7b;

  matrix.setFont(font);

  int16_t x1 = x0;
  int16_t y1 = y0 - h + 1;

  //erase area and print next second
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);
  matrix.setCursor(x0, y0);
  matrix.print(chr);

  //copy to buffer
  uint16_t buf[w * h];
  matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

  //erase area again :)
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);

  static int16_t offset = 0;
  uint16_t index = 0;

  for (uint16_t yTemp = 0; yTemp < h; yTemp++)
  {
    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
    {
      if (yTemp >= h - offset)
      {
        index = w * yTemp + xTemp;
        matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
      }
      else
      {
        //matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
      }
    }
  }
  offset++;

  //reset offset and change to next animation
  if (offset > h)
  {
    offset = 0;
    return true;
  }
  else
  {
    return false;
  }
}

boolean slide_in_down_min_digit_2(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
  //FONT
  //font = &FreeSerifBold9pt7b;
  //font = &UbuntuMono_B10pt8b;
  //font = &bold_led_board_7_regular4pt7b;
  //font = &RepetitionScrolling5pt8b;
  //font = &ChessType9pt7b;
  //font = &Org_01;
  //font = &FreeMonoBold12pt7b;
  //font = &FreeSansBold9pt7b;
  //font = &FreeSans9pt7b;

  matrix.setFont(font);

  int16_t x1 = x0;
  int16_t y1 = y0 - h + 1;

  //erase area and print next second
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);
  matrix.setCursor(x0, y0);
  matrix.print(chr);

  //copy to buffer
  uint16_t buf[w * h];
  matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

  //erase area again :)
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);

  static int16_t offset = 0;
  uint16_t index = 0;

  for (uint16_t yTemp = 0; yTemp < h; yTemp++)
  {
    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
    {
      if (yTemp >= h - offset)
      {
        index = w * yTemp + xTemp;
        matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
      }
      else
      {
        //matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
      }
    }
  }
  offset++;

  //reset offset and change to next animation
  if (offset > h)
  {
    offset = 0;
    return true;
  }
  else
  {
    return false;
  }
}

boolean slide_in_down_hour_digit_1(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
  //FONT
  //font = &FreeSerifBold9pt7b;
  //font = &UbuntuMono_B10pt8b;
  //font = &bold_led_board_7_regular4pt7b;
  //font = &RepetitionScrolling5pt8b;
  //font = &ChessType9pt7b;
  //font = &Org_01;
  //font = &FreeMonoBold12pt7b;
  //font = &FreeSansBold9pt7b;
  //font = &FreeSans9pt7b;

  matrix.setFont(font);

  int16_t x1 = x0;
  int16_t y1 = y0 - h + 1;

  //erase area and print next second
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);
  matrix.setCursor(x0, y0);
  matrix.print(chr);

  //copy to buffer
  uint16_t buf[w * h];
  matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

  //erase area again :)
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);

  static int16_t offset = 0;
  uint16_t index = 0;

  for (uint16_t yTemp = 0; yTemp < h; yTemp++)
  {
    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
    {
      if (yTemp >= h - offset)
      {
        index = w * yTemp + xTemp;
        matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
      }
      else
      {
        //matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
      }
    }
  }
  offset++;

  //reset offset and change to next animation
  if (offset > h)
  {
    offset = 0;
    return true;
  }
  else
  {
    return false;
  }
}

boolean slide_in_down_hour_digit_2(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
  //FONT
  //font = &FreeSerifBold9pt7b;
  //font = &UbuntuMono_B10pt8b;
  //font = &bold_led_board_7_regular4pt7b;
  //font = &RepetitionScrolling5pt8b;
  //font = &ChessType9pt7b;
  //font = &Org_01;
  //font = &FreeMonoBold12pt7b;
  //font = &FreeSansBold9pt7b;
  //font = &FreeSans9pt7b;

  matrix.setFont(font);

  int16_t x1 = x0;
  int16_t y1 = y0 - h + 1;

  //erase area and print next second
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);
  matrix.setCursor(x0, y0);
  matrix.print(chr);

  //copy to buffer
  uint16_t buf[w * h];
  matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

  //erase area again :)
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);

  static int16_t offset = 0;
  uint16_t index = 0;

  for (uint16_t yTemp = 0; yTemp < h; yTemp++)
  {
    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
    {
      if (yTemp >= h - offset)
      {
        index = w * yTemp + xTemp;
        matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
      }
      else
      {
        //matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
      }
    }
  }
  offset++;

  //reset offset and change to next animation
  if (offset > h)
  {
    offset = 0;
    return true;
  }
  else
  {
    return false;
  }
}

boolean slide_out_down_sec_digit_1(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
  //FONT
  //font = &FreeSerifBold9pt7b;
  //font = &UbuntuMono_B10pt8b;
  //font = &bold_led_board_7_regular4pt7b;
  //font = &RepetitionScrolling5pt8b;
  //font = &ChessType9pt7b;
  //font = &Org_01;
  //font = &FreeMonoBold12pt7b;
  //font = &FreeSansBold9pt7b;
  //font = &FreeSans9pt7b;

  matrix.setFont(font);

  int16_t x1 = x0;
  int16_t y1 = y0 - h + 1;

  //erase area and print next second
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);
  matrix.setCursor(x0, y0);
  matrix.print(chr);

  //copy to buffer
  uint16_t buf[w * h];
  matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

  //erase area again :)
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);

  static int16_t offset = 0;
  uint16_t index = 0;

  for (uint16_t yTemp = 0; yTemp < h; yTemp++)
  {
    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
    {
      if (yTemp < h - offset)
      {
        index = w * yTemp + xTemp;
        matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
      }
      else
      {
        // matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
      }
    }
  }
  offset++;

  //reset offset and change to next animation
  if (offset > h)
  {
    offset = 0;
    return true;
  }
  else
  {
    return false;
  }
}

boolean slide_out_down_sec_digit_2(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
  //FONT
  //font = &FreeSerifBold9pt7b;
  //font = &UbuntuMono_B10pt8b;
  //font = &bold_led_board_7_regular4pt7b;
  //font = &RepetitionScrolling5pt8b;
  //font = &ChessType9pt7b;
  //font = &Org_01;
  //font = &FreeMonoBold12pt7b;
  //font = &FreeSansBold9pt7b;
  //font = &FreeSans9pt7b;

  matrix.setFont(font);

  int16_t x1 = x0;
  int16_t y1 = y0 - h + 1;

  //erase area and print next second
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);
  matrix.setCursor(x0, y0);
  matrix.print(chr);

  //copy to buffer
  uint16_t buf[w * h];
  matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

  //erase area again :)
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);

  static int16_t offset = 0;
  uint16_t index = 0;

  for (uint16_t yTemp = 0; yTemp < h; yTemp++)
  {
    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
    {
      if (yTemp < h - offset)
      {
        index = w * yTemp + xTemp;
        matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
      }
      else
      {
        // matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
      }
    }
  }
  offset++;

  //reset offset and change to next animation
  if (offset > h)
  {
    offset = 0;
    return true;
  }
  else
  {
    return false;
  }
}

boolean slide_out_down_min_digit_1(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
  //FONT
  //font = &FreeSerifBold9pt7b;
  //font = &UbuntuMono_B10pt8b;
  //font = &bold_led_board_7_regular4pt7b;
  //font = &RepetitionScrolling5pt8b;
  //font = &ChessType9pt7b;
  //font = &Org_01;
  //font = &FreeMonoBold12pt7b;
  //font = &FreeSansBold9pt7b;
  //font = &FreeSans9pt7b;

  matrix.setFont(font);

  int16_t x1 = x0;
  int16_t y1 = y0 - h + 1;

  //erase area and print next second
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);
  matrix.setCursor(x0, y0);
  matrix.print(chr);

  //copy to buffer
  uint16_t buf[w * h];
  matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

  //erase area again :)
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);

  static int16_t offset = 0;
  uint16_t index = 0;

  for (uint16_t yTemp = 0; yTemp < h; yTemp++)
  {
    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
    {
      if (yTemp < h - offset)
      {
        index = w * yTemp + xTemp;
        matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
      }
      else
      {
        // matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
      }
    }
  }
  offset++;

  //reset offset and change to next animation
  if (offset > h)
  {
    offset = 0;
    return true;
  }
  else
  {
    return false;
  }
}

boolean slide_out_down_min_digit_2(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
  //FONT
  //font = &FreeSerifBold9pt7b;
  //font = &UbuntuMono_B10pt8b;
  //font = &bold_led_board_7_regular4pt7b;
  //font = &RepetitionScrolling5pt8b;
  //font = &ChessType9pt7b;
  //font = &Org_01;
  //font = &FreeMonoBold12pt7b;
  //font = &FreeSansBold9pt7b;
  //font = &FreeSans9pt7b;

  matrix.setFont(font);

  int16_t x1 = x0;
  int16_t y1 = y0 - h + 1;

  //erase area and print next second
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);
  matrix.setCursor(x0, y0);
  matrix.print(chr);

  //copy to buffer
  uint16_t buf[w * h];
  matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

  //erase area again :)
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);

  static int16_t offset = 0;
  uint16_t index = 0;

  for (uint16_t yTemp = 0; yTemp < h; yTemp++)
  {
    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
    {
      if (yTemp < h - offset)
      {
        index = w * yTemp + xTemp;
        matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
      }
      else
      {
        // matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
      }
    }
  }
  offset++;

  //reset offset and change to next animation
  if (offset > h)
  {
    offset = 0;
    return true;
  }
  else
  {
    return false;
  }
}

boolean slide_out_down_hour_digit_1(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
  //FONT
  //font = &FreeSerifBold9pt7b;
  //font = &UbuntuMono_B10pt8b;
  //font = &bold_led_board_7_regular4pt7b;
  //font = &RepetitionScrolling5pt8b;
  //font = &ChessType9pt7b;
  //font = &Org_01;
  //font = &FreeMonoBold12pt7b;
  //font = &FreeSansBold9pt7b;
  //font = &FreeSans9pt7b;

  matrix.setFont(font);

  int16_t x1 = x0;
  int16_t y1 = y0 - h + 1;

  //erase area and print next second
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);
  matrix.setCursor(x0, y0);
  matrix.print(chr);

  //copy to buffer
  uint16_t buf[w * h];
  matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

  //erase area again :)
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);

  static int16_t offset = 0;
  uint16_t index = 0;

  for (uint16_t yTemp = 0; yTemp < h; yTemp++)
  {
    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
    {
      if (yTemp < h - offset)
      {
        index = w * yTemp + xTemp;
        matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
      }
      else
      {
        // matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
      }
    }
  }
  offset++;

  //reset offset and change to next animation
  if (offset > h)
  {
    offset = 0;
    return true;
  }
  else
  {
    return false;
  }
}

boolean slide_out_down_hour_digit_2(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
  //FONT
  //font = &FreeSerifBold9pt7b;
  //font = &UbuntuMono_B10pt8b;
  //font = &bold_led_board_7_regular4pt7b;
  //font = &RepetitionScrolling5pt8b;
  //font = &ChessType9pt7b;
  //font = &Org_01;
  //font = &FreeMonoBold12pt7b;
  //font = &FreeSansBold9pt7b;
  //font = &FreeSans9pt7b;

  matrix.setFont(font);

  int16_t x1 = x0;
  int16_t y1 = y0 - h + 1;

  //erase area and print next second
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);
  matrix.setCursor(x0, y0);
  matrix.print(chr);

  //copy to buffer
  uint16_t buf[w * h];
  matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

  //erase area again :)
  matrix.fillRect(x0, y0 - h + 1, w, h, 0);

  static int16_t offset = 0;
  uint16_t index = 0;

  for (uint16_t yTemp = 0; yTemp < h; yTemp++)
  {
    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
    {
      if (yTemp < h - offset)
      {
        index = w * yTemp + xTemp;
        matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
      }
      else
      {
        // matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
      }
    }
  }
  offset++;

  //reset offset and change to next animation
  if (offset > h)
  {
    offset = 0;
    return true;
  }
  else
  {
    return false;
  }
}

void process_runningled_page()
{
  // if (MODE != MODE_old)
  // {
  //   MODE_old = MODE;
  //   digitalClockDisplay();
  //   Serial.print(F("> "));
  //   if (MODE == 0)
  //   {
  //     Serial.println(F("MODE 0 [Normal Mode]"));
  //   }
  //   else if (MODE == 1)
  //   {
  //     Serial.println(F("MODE 1 [Config Mode]"));
  //   }
  //   else if (MODE == 2)
  //   {
  //     Serial.println(F("MODE 2 [Edit Mode]"));
  //   }
  // }

  if (MODE == 0)
  {
    if (MODE != MODE_old)
    {
      MODE_old = MODE;
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.println(F("MODE 0 [Normal Mode]"));
    }

    // static bool refresh;
    if (currentPageMode0_old != currentPageMode0)
    {
      currentPageMode0_old = currentPageMode0;
      digitalClockDisplay();
      Serial.print(F("> NORMAL MODE, PAGE "));
      Serial.println(currentPageMode0);

      // refresh = true;
      matrix.clearDisplay();
    }

    // -------------------------------------------------------------------
    // MODE 0, PAGE 0, JAM DAN TANGGAL
    // -------------------------------------------------------------------
    if (currentPageMode0 == 0)
    {

      matrix.clearDisplay();

      matrix.setTextWrap(0);

      int16_t x0Temp, y0Temp, x1Temp, y1Temp;
      uint16_t w0Temp, h0Temp, w1Temp, h1Temp;

      int c;
      int row0 = 6;
      int row1 = 15;
      int startCol = 1;
      int x0 = X;
      // int x1 = X;

      if (_ledMatrixSettings.scrollrow_0 == false)
      {
        x0 = 0;
      }

      if (_ledMatrixSettings.scrollrow_1 == false)
      {
        // x1 = 0;
      }

      // -------------------------------------------------------------------
      // ROW 0
      // -------------------------------------------------------------------

      startCol = 15; //override start colum for Row0

      //hour
      c = DISP_REFF + startCol + x0;
      matrix.setFont(&bold_led_board_7_regular4pt7b);
      matrix.setCursor(c, row0);
      if (hour(local_time()) < 10)
      {
        matrix.print("0");
      }
      matrix.print(hour(local_time()));

      //colon depan
      char bufHour[2];
      dtostrf(hour(local_time()), 2, 0, bufHour);
      matrix.getTextBounds(bufHour, 0, 0, &x0Temp, &y0Temp, &w0Temp, &h0Temp);
      c = c + w0Temp + 1;
      matrix.setFont(&bold_led_board_7_regular4pt7b);
      matrix.setCursor(c, row0);

      if (second(local_time()) < 30)
      {
        matrix.fillRect(c, row0 - 1, 2, 2, 1);

        // if (blinkColon) {
        if (state1000ms)
        {
          matrix.fillRect(c, row0 - 4, 2, 2, 1);
        }
        // }
      }
      else
      {
        matrix.fillRect(c, row0 - 4, 2, 2, 1);

        // if (blinkColon) {
        if (state1000ms)
        {
          matrix.fillRect(c, row0 - 1, 2, 2, 1);
        }
        // }
      }

      //minute
      matrix.getTextBounds((char *)":", 0, 0, &x0Temp, &y0Temp, &w0Temp, &h0Temp);
      c = c + w0Temp + 1;
      matrix.setFont(&bold_led_board_7_regular4pt7b);
      matrix.setCursor(c, row0);
      if (minute(local_time()) < 10)
      {
        matrix.print("0");
      }
      matrix.print(minute(local_time()));

      // -------------------------------------------------------------------
      // ROW 1
      // -------------------------------------------------------------------
      int weekDay = weekday(localTime);
      int digitDay = day(localTime);
      int digitMonth = month(localTime);
      int digitYear = year(localTime);
      // char weekdayBuf[1];
      char dayBuf[2];
      char monthBuf[2];
      char yearBuf[4];
      itoa(digitDay, dayBuf, 10);
      itoa(digitMonth, monthBuf, 10);
      itoa(digitYear, yearBuf, 10);

      static int page;
      int pageMax = 9;

      //Construct the strings
      char contents[32];
      if (page == 0)
      {
        sprintf(contents, "%s", _configLocation.city);
      }
      if (page == 1)
      {
        sprintf(contents, "%s", hariStr(weekDay));
      }
      else if (page == 2)
      {
        sprintf(contents, "%d-%s", day(local_time()), monthShortStr(month(local_time())));
      }
      else if (page == 3)
      {
        //String degSymbol = "\xB0";
        sprintf(contents, "%s%cC", bufTempSensor, char(176));
      }
      else if (page == 4)
      {
        sprintf(contents, "%s %s", sholatNameStr(Fajr), sholatTimeArray[Fajr]);
      }
      else if (page == 5)
      {
        sprintf(contents, "%s %s", sholatNameStr(Sunrise), sholatTimeArray[Sunrise]);
      }
      else if (page == 6)
      {
        sprintf(contents, "%s %s", sholatNameStr(Dhuhr), sholatTimeArray[Dhuhr]);
      }
      else if (page == 7)
      {
        sprintf(contents, "%s %s", sholatNameStr(Asr), sholatTimeArray[Asr]);
      }
      else if (page == 8)
      {
        sprintf(contents, "%s %s", sholatNameStr(Maghrib), sholatTimeArray[Maghrib]);
      }
      else if (page == 9)
      {
        sprintf(contents, "%s %s", sholatNameStr(Isha), sholatTimeArray[Isha]);
      }

      //get text bounds
      const GFXfont *font;
      font = &RepetitionScrolling5pt8b;

      matrix.setFont(font);

      if (font == &TomThumb)
      {
        row1 = 15;
      }
      else if (font == &bold_led_board_7_regular4pt7b)
      {
        row1 = 12;
      }
      else if (font == &ChessType9pt7b)
      {
        row1 = 12;
      }
      else if (font == &FiveBySeven5pt7b)
      {
        row1 = 14;
      }
      else if (font == &F14LED7pt8b)
      {
        row1 = 12;
      }
      else if (font == &RepetitionScrolling5pt8b)
      {
        row1 = 15;
      }
      else if (font == &Org_01)
      {
        row1 = 14;
      }
      else
      {
        row1 = 9;
      }

      matrix.getTextBounds(contents, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);
      //wText = w1Temp;
      wText = matrix.width();

      startCol = (matrix.width() - w1Temp) / 2;

      //Print to led matrix

      static byte animation;

      //slide-in Left
      static int x = matrix.width();
      if (animation == 0)
      {
        //x = matrix.width();
        if (x > startCol)
        {
          x--;
        }
        else if (x == startCol)
        {
          animation = 1;
        }
      }
      if (animation == 1)
      {
        static byte count;
        count++;
        if (count == 80)
        {
          count = 0;
          animation = 2;
        }
      }
      if (animation == 2)
      {
        //x = startCol;
        x--;
        if (x == -w1Temp)
        {
          animation = 0;
          x = matrix.width();
          if (page < pageMax)
          {
            page++;
          }
          else
          {
            page = 0;
          }
          //Serial.print(F("page="));
          //Serial.println(page);
        }
      }

      matrix.setCursor(x, row1);
      matrix.print(contents);
    }

    // -------------------------------------------------------------------
    // MODE 0, PAGE 1, JAM / MENIT MENUJU SHOLAT
    // -------------------------------------------------------------------
    else if (currentPageMode0 == 1)
    {

      matrix.clearDisplay();
      matrix.setTextWrap(0);
      matrix.setFont(&TomThumb);

      int16_t x1Temp, y1Temp;
      uint16_t wTemp, hTemp;

      int c;
      int row0 = 6;
      int row1 = 14;
      byte startCol = 1;
      int x0 = X;
      int x1 = X;

      if (_ledMatrixSettings.scrollrow_0 == false)
      {
        x0 = 0;
      }

      if (_ledMatrixSettings.scrollrow_1 == false)
      {
        x1 = 0;
      }

      char textMin[] = "min";

      if (HOUR != 0)
      {
        // HOUR
        c = DISP_REFF + startCol + x0;
        matrix.setCursor(c, row0);
        matrix.println(bufHOUR);
        matrix.getTextBounds(bufHOUR, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);

        // String "jam"
        char textHour[] = "jam";
        c = c + wTemp - 3;
        matrix.setCursor(c, row0);
        matrix.println(textHour);
        matrix.getTextBounds(textHour, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);

        // MINUTE
        c = c + wTemp - 1;
        matrix.setCursor(c, row0);
        matrix.println(bufMINUTE);
        matrix.getTextBounds(bufMINUTE, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);

        // String "min"
        //char textMin[] = "min";
        c = c + wTemp - 3;
        matrix.setCursor(c, row0);
        matrix.println(textMin);
        matrix.getTextBounds(textMin, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);

        //SECOND
        matrix.getTextBounds(textMin, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
        c = c + wTemp - 1;
        matrix.setCursor(c, row0);
        matrix.println(bufSECONDMATRIX);

        //String "detik"
        char textSec[] = "dtk";
        matrix.getTextBounds(bufSECONDMATRIX, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
        c = c + wTemp - 3;
        matrix.setCursor(c, row0);
        matrix.println(textSec);
      }

      else if (HOUR == 0)
      {

        matrix.clearDisplay();

        //MINUTE
        c = DISP_REFF + startCol + x0;
        matrix.setCursor(c, row0);
        matrix.println(bufMINUTE);

        //String "min"
        matrix.getTextBounds(bufMINUTE, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
        c = c + wTemp - 3;
        matrix.setCursor(c, row0);
        matrix.println(textMin);

        //SECOND
        matrix.getTextBounds(textMin, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
        c = c + wTemp - 1;
        matrix.setCursor(c, row0);
        matrix.println(bufSECONDMATRIX);

        //String "detik"
        char textSec[] = "detik";
        matrix.getTextBounds(bufSECONDMATRIX, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
        c = c + wTemp - 3;
        matrix.setCursor(c, row0);
        matrix.println(textSec);
      }

      //String Time Name
      char textTo[] = "menuju";
      c = DISP_REFF + startCol + x1;
      matrix.setCursor(c, row1);
      matrix.print(textTo);

      // TimeName[NEXTTIMEID]
      matrix.getTextBounds(textTo, 1, 14, &x1Temp, &y1Temp, &wTemp, &hTemp);
      c = c + wTemp - 2;

      int16_t x1Temp1, y1Temp1;
      uint16_t wTemp1, hTemp1;

      // char JUMUAH[] = "JUMU'AH";

      // if (weekday(local_time()) == 6 && NEXTTIMEID == Dhuhr)
      // {
      //   matrix.getTextBounds(JUMUAH, 0, 0, &x1Temp1, &y1Temp1, &wTemp1, &hTemp1);
      // }
      // else
      // {
      //   matrix.getTextBounds(sholatNameStr(NEXTTIMEID), 0, 0, &x1Temp1, &y1Temp1, &wTemp1, &hTemp1);
      // }

      matrix.getTextBounds(sholatNameStr(NEXTTIMEID), 0, 0, &x1Temp1, &y1Temp1, &wTemp1, &hTemp1);

      //animate time name
      static byte count;
      byte maxCount = 50 / (_ledMatrixSettings.scrollspeed + 1);
      if (count < maxCount)
      {
        count++;
      }
      else if (count >= maxCount)
      {
        count = 0;
      }

      static byte animation;
      static byte i;

      //scroll Up
      if (animation == 0)
      {
        if (count == 0)
        {
          row1 = row1 + hTemp1 - i;
          i++;
          if (i == hTemp1)
          {
            animation = 2;
            i = 0;
          }
        }
        else
        {
          row1 = row1 + hTemp1 - i;
        }
      }

      //scroll Down
      else if (animation == 1)
      {
        if (count == 0)
        {
          row1 = row1 + i;
          i++;
          if (i == hTemp1)
          {
            animation = 3;
            i = 0;
          }
        }
        else
        {
          row1 = row1 + i;
        }
      }

      //no animation after scroll Up
      else if (animation == 2)
      {
        static byte count;
        count++;
        if (count == 120)
        {
          count = 0;
          animation = 1;
        }
      }

      //no animation after scroll Down
      else if (animation == 3)
      {
        static byte count;
        count++;
        row1 = row1 + hTemp1;
        if (count == 20)
        {
          count = 0;
          animation = 0;
        }
      }

      matrix.setCursor(c, row1);

      // if (weekday(local_time()) == 6 && NEXTTIMEID == Dhuhr)
      // {
      //   matrix.print(JUMUAH);
      // }
      // else
      // {
      //   matrix.print(sholatNameStr(NEXTTIMEID));
      // }

      matrix.print(sholatNameStr(NEXTTIMEID));

      wText = DISP_REFF + wTemp + wTemp1;

      //matrix.fillRect(c, row1+3, wTemp1, hTemp1, 1);
      //c = matrix.getCursorX();
      matrix.fillRect(c, 14, wTemp1, hTemp1, 0);

      //matrix.fillScreen(1);
    }

    // -------------------------------------------------------------------
    // MODE 0, PAGE 2, WAKTU SHOLAT SAAT INI
    // -------------------------------------------------------------------
    else if (currentPageMode0 == 2)
    {

      matrix.clearDisplay();
      matrix.setTextWrap(0);

      int c;
      int row0 = 6;
      int row1 = 14;
      // byte startCol = 1;
      // int x0 = X;
      // int x1 = X;

      // if (_ledMatrixSettings.scrollrow_0 == false)
      // {
      //   x0 = 0;
      // }

      // if (_ledMatrixSettings.scrollrow_1 == false)
      // {
      //   x1 = 0;
      // }

      // -------------------------------------------------------------------
      // ROW 1
      // -------------------------------------------------------------------

      char buf[15];
      const char *strRow1 = PSTR("MASUK WAKTU");

      strcpy_P(buf, strRow1);

      int16_t x0Temp, y0Temp;
      uint16_t w0Temp, h0Temp;

      matrix.setFont(&TomThumb);
      matrix.getTextBounds(buf, 0, 0, &x0Temp, &y0Temp, &w0Temp, &h0Temp);
      //wText = w0Temp;

      //center
      c = (64 - w0Temp) / 2;

      //Print to led matrix
      matrix.setCursor(c, row0);
      matrix.print(buf);

      // -------------------------------------------------------------------
      // ROW 2
      // -------------------------------------------------------------------

      strlcpy(buf, sholatNameStr(CURRENTTIMEID), sizeof(buf));

      int16_t x1Temp, y1Temp;
      uint16_t w1Temp, h1Temp;

      matrix.setFont(&bold_led_board_7_regular4pt7b);
      matrix.getTextBounds(buf, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);
      //wText = w0Temp;

      //center
      c = (64 - w1Temp) / 2;

      //Print to led matrix
      matrix.setCursor(c, row1);
      matrix.print(buf);
    }

    // -------------------------------------------------------------------
    // MODE 0, PAGE 3, JAM SAJA
    // -------------------------------------------------------------------
    else if (currentPageMode0 == 3)
    {
      byte typePage3 = 1;

      if (typePage3 == 0)
      {
        matrix.clearDisplay();
        matrix.setTextWrap(0);
        matrix.setFont(&TomThumb);

        // int16_t x1Temp, y1Temp;
        // uint16_t wTemp, hTemp;

        int c;
        // int row0 = 6;
        int row1 = 13;
        byte startCol = 1;

        c = DISP_REFF + startCol;

        //matrix.setFont(&UbuntuMono_B10pt8b);
        //matrix.setFont(&bold_led_board_7_regular4pt7b);
        //matrix.setFont(&RepetitionScrolling5pt8b);
        //matrix.setFont(&ChessType9pt7b);
        //matrix.setFont(&FreeSerifBold9pt7b);
        //matrix.setFont(&Org_01);
        //matrix.setFont(&FreeMonoBold12pt7b);
        //matrix.setFont(&FreeSansBold9pt7b);
        //matrix.setFont(&FreeSans9pt7b);

        const GFXfont *font;

        font = &FreeSerifBold9pt7b;

        matrix.setFont(font);

        //break time element for NOW time
        time_t t = local_time();
        tmElements_t tmSet;
        breakTime(t, tmSet);

        //array for time Now
        char bufNow[6];

        //format (add leading '0' if  less than 10) and store in the array
        sprintf(bufNow, "%02d%02d%02d", tmSet.Hour, tmSet.Minute, tmSet.Second);

        // print Colon
        matrix.fillRect(c + 19, row1 - 9, 2, 2, 1);
        matrix.fillRect(c + 19, row1 - 3, 2, 2, 1);
        //matrix.fillRect(c + 41, row1 - 8, 2, 2, 1);
        //matrix.fillRect(c + 41, row1 - 1, 2, 2, 1);

        // print Hour
        matrix.setCursor(c, row1);
        matrix.print(bufNow[0]);
        matrix.print(bufNow[1]);

        //  //if Hour is les than 10, erase the leading zero
        //  if (bufNow[0] == '0') {
        //    matrix.fillRect(1, 2, 9, 12, 0);
        //  }

        // print Minute
        c = matrix.getCursorX();
        matrix.setCursor(c + 4, row1);
        matrix.print(bufNow[2]);
        matrix.print(bufNow[3]);

        // print Second
        c = matrix.getCursorX();

        font = &bold_led_board_7_regular4pt7b;
        matrix.setFont(font);

        matrix.setCursor(c + 4, row1);
        matrix.print(bufNow[4]);

        if (font == &FreeSerifBold9pt7b)
        {
          matrix.setCursor(c + 13, row1);
        }
        if (font == &bold_led_board_7_regular4pt7b)
        {
          matrix.setCursor(c + 12, row1);
        }

        matrix.print(bufNow[5]);

        //revert font again
        matrix.setFont(&FreeSerifBold9pt7b);

        //      // initial animation: slide-out-down
        //      static uint8_t animation = 2;

        //array for 1 second ahead
        static char bufTimeFuture[6];

        //counter to determine when to trigger the animation
        int sec;
        static int second_old = 0;
        sec = bufNow[5];
        // static unsigned long currMillis;
        static uint16_t count = 0;
        if (sec != second_old)
        {
          second_old = sec;
          // currMillis = millis();
          count = 0;
        }

        static boolean animateSecDigit1;
        static boolean animateSecDigit2;
        static boolean animateMinDigit1;
        static boolean animateMinDigit2;
        static boolean animateHourDigit1;
        static boolean animateHourDigit2;

        if (count == 1000 / _ledMatrixSettings.scrollspeed - 25)
        {
          //break time element
          time_t t_future = local_time() + 1;
          tmElements_t tmSet_future;
          breakTime(t_future, tmSet_future);

          //format (add leading '0' if less than 10) and store in the array
          sprintf(bufTimeFuture, "%02d%02d%02d", tmSet_future.Hour, tmSet_future.Minute, tmSet_future.Second);

          //animateSecDigit2 = true;

          if (bufNow[5] == '9')
          {
            //animateSecDigit1 = true;
            if (bufNow[4] == '5')
            {
              animateMinDigit2 = true;
              if (bufNow[3] == '9')
              {
                animateMinDigit1 = true;
                if (bufNow[2] == '5')
                {
                  animateHourDigit2 = true;
                  if (bufNow[1] == '3' || bufNow[1] == '9')
                  {
                    animateHourDigit1 = true;
                  }
                }
              }
            }
          }
        }

        if (count == 1000 / _ledMatrixSettings.scrollspeed - 15)
        {

          animateSecDigit2 = true;

          if (bufNow[5] == '9')
          {
            animateSecDigit1 = true;
          }
        }

        count++;

        if (animateSecDigit2)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_sec_digit_2(bufNow[5], &bold_led_board_7_regular4pt7b, 53, 13, 7, 7);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_sec_digit_2(bufTimeFuture[5], &bold_led_board_7_regular4pt7b, 53, 13, 7, 7);
            if (slideInDown == true)
            {
              animateSecDigit2 = false;
              animation = 0;
            }
          }
        }

        if (animateSecDigit1)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_sec_digit_1(bufNow[4], &bold_led_board_7_regular4pt7b, 45, 13, 7, 7);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_sec_digit_1(bufTimeFuture[4], &bold_led_board_7_regular4pt7b, 45, 13, 7, 7);
            if (slideInDown == true)
            {
              animateSecDigit1 = false;
              animation = 0;
            }
          }
        }

        if (animateMinDigit2)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_min_digit_2(bufNow[3], &FreeSerifBold9pt7b, 32, 13, 9, 12);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_min_digit_2(bufTimeFuture[3], &FreeSerifBold9pt7b, 32, 13, 9, 12);
            if (slideInDown == true)
            {
              animateMinDigit2 = false;
              animation = 0;
            }
          }
        }

        if (animateMinDigit1)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_min_digit_1(bufNow[2], &FreeSerifBold9pt7b, 23, 13, 9, 12);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_min_digit_1(bufTimeFuture[2], &FreeSerifBold9pt7b, 23, 13, 9, 12);
            if (slideInDown == true)
            {
              animateMinDigit1 = false;
              animation = 0;
            }
          }
        }

        if (animateHourDigit2)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_hour_digit_2(bufNow[1], &FreeSerifBold9pt7b, 10, 13, 9, 12);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_hour_digit_2(bufTimeFuture[1], &FreeSerifBold9pt7b, 10, 13, 9, 12);
            if (slideInDown == true)
            {
              animateHourDigit2 = false;
              animation = 0;
            }
          }
        }

        if (animateHourDigit1)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_hour_digit_1(bufNow[0], &FreeSerifBold9pt7b, 1, 13, 9, 12);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_hour_digit_1(bufTimeFuture[0], &FreeSerifBold9pt7b, 1, 13, 9, 12);
            if (slideInDown == true)
            {
              animateHourDigit1 = false;
              animation = 0;
            }
          }
        }
      }

      else if (typePage3 == 1)
      {
        matrix.clearDisplay();
        matrix.setTextWrap(0);
        matrix.setFont(&TomThumb);

        // int16_t x1Temp, y1Temp;
        // uint16_t wTemp, hTemp;

        int c;
        int row0 = 6;
        int row1 = 14;
        byte startCol = 1;

        c = DISP_REFF + startCol;

        //matrix.setFont(&UbuntuMono_B10pt8b);
        //matrix.setFont(&bold_led_board_7_regular4pt7b);
        //matrix.setFont(&RepetitionScrolling5pt8b);
        //matrix.setFont(&ChessType9pt7b);
        //matrix.setFont(&FreeSerifBold9pt7b);
        //matrix.setFont(&Org_01);
        //matrix.setFont(&FreeMonoBold12pt7b);
        //matrix.setFont(&FreeSansBold9pt7b);
        //matrix.setFont(&FreeSans9pt7b);
        //matrix.setFont(&FreeSerifBoldNumberOnly11pt7b);

        const GFXfont *font;

        font = &FreeSerifBoldNumberOnly11pt7b;

        matrix.setFont(font);

        //break time element for NOW time
        time_t t = local_time();
        tmElements_t tmSet;
        breakTime(t, tmSet);

        //array for time Now
        char bufNow[6];

        //format (add leading '0' if  less than 10) and store in the array
        sprintf(bufNow, "%02d%02d%02d", tmSet.Hour, tmSet.Minute, tmSet.Second);

        // print Colon
        matrix.fillRect(c + 22, row1 - 9, 2, 2, 1);
        matrix.fillRect(c + 22, row1 - 4, 2, 2, 1);
        //matrix.fillRect(c + 41, row1 - 8, 2, 2, 1);
        //matrix.fillRect(c + 41, row1 - 1, 2, 2, 1);

        // print Hour
        matrix.setCursor(c, row1);
        matrix.print(bufNow[0]);
        matrix.print(bufNow[1]);

        //  //if Hour is les than 10, erase the leading zero
        //  if (bufNow[0] == '0') {
        //    matrix.fillRect(1, 2, 9, 12, 0);
        //  }

        // print Minute
        c = matrix.getCursorX();
        matrix.setCursor(c + 3, row1);
        matrix.print(bufNow[2]);
        matrix.print(bufNow[3]);

        // print Second
        c = matrix.getCursorX();

        font = &bold_led_board_7_regular4pt7b;
        matrix.setFont(font);

        matrix.setCursor(c, row1);
        matrix.print(bufNow[4]);

        if (font == &FreeSerifBold9pt7b)
        {
          matrix.setCursor(c + 13, row1);
        }
        if (font == &bold_led_board_7_regular4pt7b)
        {
          matrix.setCursor(c + 8, row1);
        }
        matrix.print(bufNow[5]);

        // print AM/PM
        matrix.setCursor(c, row0);
        if (isAM(local_time()))
        {
          //matrix.print("AM");
        }
        else if (isPM(local_time()))
        {
          //matrix.print("PM");
        }

        //revert font again
        font = &FreeSerifBoldNumberOnly11pt7b;
        matrix.setFont(font);

        //array for 1 second ahead
        static char bufTimeFuture[6];

        //counter to determine when to trigger the animation
        int sec;
        static int second_old = 0;
        sec = bufNow[5];
        // static unsigned long currMillis;
        static uint16_t count = 0;
        if (sec != second_old)
        {
          second_old = sec;
          // currMillis = millis();
          count = 0;
        }

        static boolean animateSecDigit1;
        static boolean animateSecDigit2;
        static boolean animateMinDigit1;
        static boolean animateMinDigit2;
        static boolean animateHourDigit1;
        static boolean animateHourDigit2;

        if (count == 1000 / _ledMatrixSettings.scrollspeed - 31)
        {
          //break time element
          time_t t_future = local_time() + 1;
          tmElements_t tmSet_future;
          breakTime(t_future, tmSet_future);

          //format (add leading '0' if less than 10) and store in the array
          sprintf(bufTimeFuture, "%02d%02d%02d", tmSet_future.Hour, tmSet_future.Minute, tmSet_future.Second);

          //animateSecDigit2 = true;

          if (bufNow[5] == '9')
          {
            //animateSecDigit1 = true;
            if (bufNow[4] == '5')
            {
              animateMinDigit2 = true;
              if (bufNow[3] == '9')
              {
                animateMinDigit1 = true;
                if (bufNow[2] == '5')
                {
                  animateHourDigit2 = true;
                  if (bufNow[1] == '3' || bufNow[1] == '9')
                  {
                    animateHourDigit1 = true;
                  }
                }
              }
            }
          }
        }

        if (count == 1000 / _ledMatrixSettings.scrollspeed - 15)
        {

          animateSecDigit2 = true;

          if (bufNow[5] == '9')
          {
            animateSecDigit1 = true;
          }
        }

        count++;

        if (animateSecDigit2)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_sec_digit_2(bufNow[5], &bold_led_board_7_regular4pt7b, 56, 14, 7, 7);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_sec_digit_2(bufTimeFuture[5], &bold_led_board_7_regular4pt7b, 56, 14, 7, 7);
            if (slideInDown == true)
            {
              animateSecDigit2 = false;
              animation = 0;
            }
          }
        }

        if (animateSecDigit1)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_sec_digit_1(bufNow[4], &bold_led_board_7_regular4pt7b, 48, 14, 7, 7);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_sec_digit_1(bufTimeFuture[4], &bold_led_board_7_regular4pt7b, 48, 14, 7, 7);
            if (slideInDown == true)
            {
              animateSecDigit1 = false;
              animation = 0;
            }
          }
        }

        if (animateMinDigit2)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_min_digit_2(bufNow[3], &FreeSerifBoldNumberOnly11pt7b, 37, 14, 11, 15);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_min_digit_2(bufTimeFuture[3], &FreeSerifBoldNumberOnly11pt7b, 37, 14, 11, 15);
            if (slideInDown == true)
            {
              animateMinDigit2 = false;
              animation = 0;
            }
          }
        }

        if (animateMinDigit1)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_min_digit_1(bufNow[2], &FreeSerifBoldNumberOnly11pt7b, 26, 14, 11, 15);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_min_digit_1(bufTimeFuture[2], &FreeSerifBoldNumberOnly11pt7b, 26, 14, 11, 15);
            if (slideInDown == true)
            {
              animateMinDigit1 = false;
              animation = 0;
            }
          }
        }

        if (animateHourDigit2)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_hour_digit_2(bufNow[1], &FreeSerifBoldNumberOnly11pt7b, 12, 14, 11, 15);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_hour_digit_2(bufTimeFuture[1], &FreeSerifBoldNumberOnly11pt7b, 12, 14, 11, 15);
            if (slideInDown == true)
            {
              animateHourDigit2 = false;
              animation = 0;
            }
          }
        }

        if (animateHourDigit1)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_hour_digit_1(bufNow[0], &FreeSerifBoldNumberOnly11pt7b, 1, 14, 11, 15);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_hour_digit_1(bufTimeFuture[0], &FreeSerifBoldNumberOnly11pt7b, 1, 14, 11, 15);
            if (slideInDown == true)
            {
              animateHourDigit1 = false;
              animation = 0;
            }
          }
        }
      }
    }

    // -------------------------------------------------------------------
    // MODE 0, PAGE 4, KWH 1
    // -------------------------------------------------------------------
    else if (currentPageMode0 == 4)
    {

      matrix.clearDisplay();

      matrix.setTextWrap(0);

      // int16_t x0Temp;
      // int16_t y0Temp;
      // int16_t x1Temp;
      // int16_t y1Temp;
      uint16_t w0Temp;
      // uint16_t h0Temp;
      uint16_t w1Temp;
      // uint16_t h1Temp;

      int c;
      int row0 = 7;
      int row1 = 15;
      byte startCol = 1;
      int x0 = X;
      int x1 = X;

      if (_ledMatrixSettings.scrollrow_0 == false)
      {
        x0 = 0;
      }

      if (_ledMatrixSettings.scrollrow_1 == false)
      {
        x1 = 0;
      }

      // Wattage
      c = DISP_REFF + startCol - 1 + x0;
      matrix.setFont(&FiveBySeven5pt7b);
      matrix.setCursor(c, row0);
      matrix.print(bufWatt);
      //matrix.getTextBounds(bufWatt, 0, 0, &x0Temp, &y0Temp, &w0Temp, &h0Temp);

      // String "Watt"
      char textWatt[] = "Watt";
      c = DISP_REFF + startCol + x1;
      matrix.setFont(&TomThumb);
      matrix.setCursor(c, row1);
      matrix.println(textWatt);
      //matrix.getTextBounds(textWatt, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);

      // Voltage
      c = c + 30 + x0;
      matrix.setFont(&TomThumb);
      matrix.setCursor(c, row0);
      matrix.print(bufVoltage);
      //w0Temp = matrix.getCursorX();

      // String "Volt"
      char textVolt[] = "Volt";
      c = c + -3 + x1;
      matrix.setFont(&TomThumb);
      matrix.setCursor(c, row1);
      matrix.print(textVolt);
      //matrix.getTextBounds(textWatt, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);
      //w1Temp = matrix.getCursorX();

      // Temperature
      c = c + 21 + x0;
      matrix.setFont(&TomThumb);
      matrix.setCursor(c, row0);
      matrix.print(bufTempSensor);
      w0Temp = matrix.getCursorX();

      // String "degC"
      char textDegC[] = "degC";
      c = c + -1 + x1;
      matrix.setFont(&TomThumb);
      matrix.setCursor(c, row1);
      matrix.print(textDegC);
      //matrix.getTextBounds(textWatt, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);
      w1Temp = matrix.getCursorX();

      if (w0Temp > w1Temp)
      {
        wText = w0Temp;
      }
      else
      {
        wText = w1Temp;
      }
    }

    // -------------------------------------------------------------------
    // MODE 0, PAGE 5, LONG TEXT
    // -------------------------------------------------------------------
    else if (currentPageMode0 == 5)
    {

      matrix.setTextWrap(0);

      // int16_t x1Temp, y1Temp;
      // uint16_t wTemp, hTemp;

      int c;
      // int row0 = 6;
      // int row1 = 14;
      byte startCol = 1;

      if (_ledMatrixSettings.scrollrow_0 == false)
      {
        c = DISP_REFF + startCol;
      }

      if (_ledMatrixSettings.scrollrow_1 == false)
      {
        c = DISP_REFF + startCol;
      }

      c = DISP_REFF + startCol + X;

      matrix.clearDisplay();

      matrix.setTextSize(1);

      const GFXfont *font = &F14LED7pt8b;
      matrix.setFont(font);

      if (font == &TomThumb)
      {
        matrix.setCursor(c, 14);
      }
      else if (font == &bold_led_board_7_regular4pt7b)
      {
        matrix.setCursor(c, 12);
      }
      else if (font == &ChessType9pt7b)
      {
        matrix.setCursor(c, 12);
      }
      else if (font == &FiveBySeven5pt7b)
      {
        matrix.setCursor(c, 14);
      }
      else if (font == &F14LED7pt8b)
      {
        matrix.setCursor(c, 12);
      }

      load_running_text();

      // -------------------------------------------------------------------
      // Animation
      // -------------------------------------------------------------------
      boolean effect0 = 0;
      // boolean effect1 = 0;

      if (effect0 == 1)
      {
        static uint16_t scan = 0;
        //uint16_t scanMax = _ledMatrixSettings.scrollspeed / 7;
        uint16_t scanMax = _ledMatrixSettings.scrollspeed;
        if (scan < scanMax)
        {
          scan++;
        }
        else if (scan >= scanMax)
        {
          scan = 0;
        }

        //    int16_t x0 = 2;
        //    int16_t y0 = 3;
        //    int16_t x1 = 2;
        //    int16_t y1 = 3;
        //    uint16_t w = 14;
        //    uint16_t h = 10;
      }

      if (effect0 == 1)
      {
        int16_t x0 = 0;
        int16_t y0 = 0;
        int16_t x1 = 0;
        int16_t y1 = 0;
        uint16_t w = 64;
        uint16_t h = 16;

        static int16_t offset;
        static uint16_t offsetMax;

        static uint8_t animation = 6;

        uint16_t index;
        uint16_t buf[1024];

        matrix.copyBuffer(x0, y0, w, h, buf);
        matrix.fillRect(x0, y0, w, h, 0);

        for (uint16_t yTemp = 0; yTemp < h; yTemp++)
        {
          for (uint16_t xTemp = 0; xTemp < w; xTemp++)
          {

            //slide-out-up
            if (animation == 0)
            {
              offsetMax = h;
              if (yTemp >= offset)
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - offset, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - offset, 0);
              }
            }

            //slide-out-down
            else if (animation == 1)
            {
              offsetMax = h;
              if (yTemp < h - offset)
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, 0);
              }
            }

            //slide-out-right
            else if (animation == 2)
            {
              offsetMax = w;
              if (xTemp < w - offset)
              {
                matrix.drawPixel(x1 + xTemp + offset, y1 + yTemp, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp + offset, y1 + yTemp, 0);
              }
            }

            //slide-out-to-left
            else if (animation == 3)
            {
              offsetMax = w;
              if (xTemp >= offset)
              {
                matrix.drawPixel(x1 + xTemp - offset, y1 + yTemp, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp - offset, y1 + yTemp, 0);
              }
            }

            //slide-in-right
            else if (animation == 4)
            {
              offsetMax = w;
              if (xTemp >= w - offset)
              {
                matrix.drawPixel(x1 + xTemp - w + offset, y1 + yTemp, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp - w + offset, y1 + yTemp, 0);
              }
            }

            //slide-in-up
            else if (animation == 5)
            {
              offsetMax = h;
              if (yTemp < offset)
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + h - offset, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + h - offset, 0);
              }
            }

            //slide-in-down
            else if (animation == 6)
            {
              offsetMax = h;
              if (yTemp >= h - offset)
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
              }
            }

            //slide-in-left
            else if (animation == 7)
            {
              offsetMax = w;
              if (xTemp < offset)
              {
                matrix.drawPixel(x1 + xTemp + w - offset, y1 + yTemp, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp + w - offset, y1 + yTemp, 0);
              }
            }

            index++;
          }
        }

        offset++;

        //reset offset and change animation
        if (offset > offsetMax)
        {
          offset = 0;
          if (animation == 6)
          {
            animation = 3;
          }
          else if (animation == 3)
          {
            animation = 4;
          }
          else if (animation == 4)
          {
            animation = 1;
          }
          else if (animation == 1)
          {
            animation = 5;
          }
          else if (animation == 5)
          {
            animation = 2;
          }
          else if (animation == 2)
          {
            animation = 7;
          }
          else if (animation == 7)
          {
            animation = 0;
          }
          else if (animation == 0)
          {
            animation = 6;
          }
        }
      }
    }
    // -------------------------------------------------------------------
    // MODE 0, PAGE 6, TEMPERATURE DAN JAM
    // -------------------------------------------------------------------
    else if (currentPageMode0 == 6)
    {

      matrix.clearDisplay();
      matrix.setTextWrap(0);

      // int16_t x1Temp, y1Temp;
      // uint16_t w1Temp, h1Temp;

      // char *str;
      // int16_t stopCol;
      // int16_t stopRow;
      // const GFXfont *font;

      uint8_t pageMax = 1;

      static uint8_t exitAnimation = 0;
      static uint8_t nextPageAnimation = 0;

      // ------------------------
      // CURRENT PAGE
      // ------------------------

      static int currentPage;

      //String strCurrentPage;

      uint8_t len = 20;
      char strCurrentPage[len];

      //Construct text
      if (currentPage == 0)
      {
        //strcpy(strCurrentPage, bufTempSensor);
        strcpy(strCurrentPage, bufRtcTemp);
        strcat(strCurrentPage, "\xB0");
        strcat(strCurrentPage, "C");
      }
      else if (currentPage == 1)
      {
        char bufHour[3];
        dtostrf(hour(local_time()), 1, 0, bufHour);
        strcpy(strCurrentPage, bufHour);

        //if (s_digit_2[0] % 2) { // do something odd
        if (blinkColon)
        {
          if (state500ms == LOW)
          {
            strcat(strCurrentPage, " ");
          }
        }
        else
        {
          strcat(strCurrentPage, ":");
        }
        if (minute(local_time()) < 10)
        {
          strcat(strCurrentPage, "0");
        }
        //strCurrentPage += minute(local_time());
        char buffer[3];
        strcat(strCurrentPage, itoa(minute(local_time()), buffer, 10));
      }
      else if (currentPage == 2)
      {
        //strCurrentPage += bufWatt;
        strcat(strCurrentPage, bufWatt);
      }

      static int x_offset_currentPage = 0;
      static int y_offset_currentPage = 0;

      // ------------------------
      // IF NO ANIMATION
      // ------------------------
      static boolean animation = 0;

      if (animation == 0)
      {
        x_offset_currentPage = 0;
        y_offset_currentPage = 0;
        static unsigned int count;
        count++;
        unsigned int waitingTime = 0;
        if (currentPage == 0)
        {
          waitingTime = 80 * 4;
        }
        else if (currentPage == 1)
        {
          //waitingTime = 80 * 30;
          waitingTime = 80 * 30;
        }
        else if (currentPage == 2)
        {
          //waitingTime = 80 * 30;
          waitingTime = 80 * 2;
        }
        if (count == waitingTime)
        {
          //reset count
          count = 0;
          animation = 1;
          nextPageAnimation = random(0, 3 + 1);
          exitAnimation = random(0, 3 + 1);
          //exitAnimation = nextPageAnimation;
        }

        int16_t x0_currentPage;
        int16_t y0_currentPage;

        if (currentPage == 0)
        {
          PreparePage(strCurrentPage, &F14LED7pt8b, &x0_currentPage, &y0_currentPage);
        }
        else if (currentPage == 1)
        {
          PreparePage(strCurrentPage, &UbuntuMono_B10pt8b, &x0_currentPage, &y0_currentPage);
        }
        if (currentPage == 2)
        {
          PreparePage(strCurrentPage, &F14LED7pt8b, &x0_currentPage, &y0_currentPage);
        }

        matrix.setCursor(x0_currentPage + x_offset_currentPage, y0_currentPage + y_offset_currentPage);
        matrix.print(strCurrentPage);
      }

      // ------------------------
      // IF ANIMATION TURNED ON
      // ------------------------

      else if (animation == 1)
      {

        // ------------------------
        // CURRENT PAGE, ANIMATION TURNED ON
        // ------------------------

        int16_t max_offset_currentPage;
        int16_t currOffsetCurrentPage;

        //SLIDE OUT RIGHT
        if (exitAnimation == 0)
        {
          x_offset_currentPage++;
          currOffsetCurrentPage = x_offset_currentPage;
          y_offset_currentPage = 0;
          max_offset_currentPage = matrix.width();
        }
        //SLIDE OUT LEFT
        else if (exitAnimation == 1)
        {
          x_offset_currentPage--;
          currOffsetCurrentPage = x_offset_currentPage;
          y_offset_currentPage = 0;
          max_offset_currentPage = -matrix.width();
        }
        //SLIDE OUT UP
        else if (exitAnimation == 2)
        {
          x_offset_currentPage = 0;
          y_offset_currentPage--;
          currOffsetCurrentPage = y_offset_currentPage;
          max_offset_currentPage = -matrix.height();
        }
        //SLIDE OUT DOWN
        else if (exitAnimation == 3)
        {
          x_offset_currentPage = 0;
          y_offset_currentPage++;
          currOffsetCurrentPage = y_offset_currentPage;
          max_offset_currentPage = matrix.height();
        }
        //SLIDE OUT RIGHT UP
        else if (exitAnimation == 4)
        {
          x_offset_currentPage++;
          y_offset_currentPage--;
          currOffsetCurrentPage = y_offset_currentPage;
          max_offset_currentPage = matrix.height();
        }
        //SLIDE OUT LEFT UP
        else if (exitAnimation == 5)
        {
          x_offset_currentPage--;
          y_offset_currentPage--;
          currOffsetCurrentPage = y_offset_currentPage;
          max_offset_currentPage = matrix.height();
        }
        //SLIDE OUT RIGHT DOWN
        else if (exitAnimation == 6)
        {
          x_offset_currentPage++;
          y_offset_currentPage++;
          currOffsetCurrentPage = y_offset_currentPage;
          max_offset_currentPage = matrix.height();
        }
        //SLIDE OUT LEFT DOWN
        else if (exitAnimation == 7)
        {
          x_offset_currentPage--;
          y_offset_currentPage++;
          currOffsetCurrentPage = y_offset_currentPage;
          max_offset_currentPage = matrix.height();
        }

        int16_t x0_currentPage;
        int16_t y0_currentPage;
        if (currentPage == 0)
        {
          PreparePage(strCurrentPage, &F14LED7pt8b, &x0_currentPage, &y0_currentPage);
        }
        else if (currentPage == 1)
        {
          PreparePage(strCurrentPage, &UbuntuMono_B10pt8b, &x0_currentPage, &y0_currentPage);
        }
        else if (currentPage == 2)
        {
          PreparePage(strCurrentPage, &F14LED7pt8b, &x0_currentPage, &y0_currentPage);
        }

        //if (x_offset_currentPage != max_offset_currentPage) {
        matrix.setCursor(x0_currentPage + x_offset_currentPage, y0_currentPage + y_offset_currentPage);
        matrix.print(strCurrentPage);
        //}

        // ------------------------
        // NEXT PAGE, ANIMATION TURNED ON
        // ------------------------

        uint8_t nextPage = currentPage + 1;
        if (nextPage > pageMax)
        {
          nextPage = 0;
        }

        //String strNextPage;
        //strNextPage = "";

        char strNextPage[len];

        //Construct text for Page 0
        if (nextPage == 0)
        {
          //strcpy(strNextPage, bufTempSensor);
          strcpy(strNextPage, bufRtcTemp);
          strcat(strNextPage, "\xB0");
          strcat(strNextPage, "C");
        }
        else if (nextPage == 1)
        {
          char bufHour[2];
          dtostrf(hour(local_time()), 1, 0, bufHour);
          strcpy(strNextPage, bufHour);

          //if (s_digit_2[0] % 2) { // do something odd
          if (blinkColon)
          {
            if (state500ms)
            {
              strcat(strNextPage, " ");
            }
          }
          else
          {
            strcat(strNextPage, ":");
          }

          if (minute(local_time()) < 10)
          {
            strcat(strNextPage, "0");
          }
          char buffer[3];
          strcat(strNextPage, itoa(minute(local_time()), buffer, 10));
        }
        else if (nextPage == 2)
        {
          strcpy(strNextPage, bufWatt);
          strcat(strNextPage, " W");
        }

        //NEXT PAGE PARAMETER
        int16_t x_center, y_center;
        static int16_t x_offset_nextPage = 0;
        static int16_t y_offset_nextPage = 0;
        int16_t currOffsetNextPage = 0;
        int16_t max_offset_nextPage;

        int16_t x0_nextPage = 0;
        int16_t y0_nextPage = 0;

        //SLIDE IN RIGHT
        if (nextPageAnimation == 0)
        {

          x_offset_nextPage++;
          currOffsetNextPage = x_offset_nextPage;
          y_offset_nextPage = 0;
          max_offset_nextPage = matrix.width();

          if (nextPage == 0)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }
          else if (nextPage == 1)
          {
            PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
          }
          else if (nextPage == 2)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }

          x0_nextPage = x_center + x_offset_nextPage - matrix.width();
          y0_nextPage = y_center + y_offset_nextPage;
        }
        //SLIDE IN LEFT
        else if (nextPageAnimation == 1)
        {

          x_offset_nextPage--;
          currOffsetNextPage = x_offset_nextPage;
          y_offset_nextPage = 0;
          max_offset_nextPage = -matrix.width();

          if (nextPage == 0)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }
          else if (nextPage == 1)
          {
            PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
          }
          else if (nextPage == 2)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }

          x0_nextPage = x_center + x_offset_nextPage + matrix.width();
          y0_nextPage = y_center + y_offset_nextPage;
        }
        //SLIDE IN UP
        else if (nextPageAnimation == 2)
        {

          x_offset_nextPage = 0;
          y_offset_nextPage--;
          currOffsetNextPage = y_offset_nextPage;
          max_offset_nextPage = -matrix.height();

          if (nextPage == 0)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }
          else if (nextPage == 1)
          {
            PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
          }
          else if (nextPage == 2)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }

          x0_nextPage = x_center + x_offset_nextPage;
          y0_nextPage = y_center + y_offset_nextPage + matrix.height();
        }
        //SLIDE IN DOWN
        else if (nextPageAnimation == 3)
        {

          x_offset_nextPage = 0;
          y_offset_nextPage++;
          currOffsetNextPage = y_offset_nextPage;
          max_offset_nextPage = matrix.height();

          if (nextPage == 0)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }
          else if (nextPage == 1)
          {
            PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
          }
          else if (nextPage == 2)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }

          x0_nextPage = x_center + x_offset_nextPage;
          y0_nextPage = y_center + y_offset_nextPage - matrix.height();
        }
        //SLIDE IN RIGHT UP
        else if (nextPageAnimation == 4)
        {

          x_offset_nextPage++;
          y_offset_nextPage--;
          currOffsetNextPage = y_offset_nextPage;
          max_offset_nextPage = -matrix.height();

          if (nextPage == 0)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }
          else if (nextPage == 1)
          {
            PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
          }
          else if (nextPage == 2)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }

          x0_nextPage = x_center + x_offset_nextPage - matrix.height();
          y0_nextPage = y_center + y_offset_nextPage + matrix.height();
        }
        //SLIDE IN LEFT UP
        else if (nextPageAnimation == 5)
        {

          x_offset_nextPage--;
          y_offset_nextPage--;
          currOffsetNextPage = y_offset_nextPage;
          max_offset_nextPage = -matrix.height();

          if (nextPage == 0)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }
          else if (nextPage == 1)
          {
            PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
          }
          else if (nextPage == 2)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }

          x0_nextPage = x_center + x_offset_nextPage + matrix.height();
          y0_nextPage = y_center + y_offset_nextPage + matrix.height();
        }
        //SLIDE IN RIGHT DOWN
        else if (nextPageAnimation == 6)
        {

          x_offset_nextPage++;
          y_offset_nextPage++;
          currOffsetNextPage = y_offset_nextPage;
          max_offset_nextPage = matrix.height();

          if (nextPage == 0)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }
          else if (nextPage == 1)
          {
            PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
          }
          else if (nextPage == 2)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }

          x0_nextPage = x_center + x_offset_nextPage - matrix.height();
          y0_nextPage = y_center + y_offset_nextPage - matrix.height();
        }
        //SLIDE IN LEFT DOWN
        else if (nextPageAnimation == 7)
        {

          x_offset_nextPage--;
          y_offset_nextPage++;
          currOffsetNextPage = y_offset_nextPage;
          max_offset_nextPage = matrix.height();

          if (nextPage == 0)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }
          else if (nextPage == 1)
          {
            PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
          }
          else if (nextPage == 2)
          {
            PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
          }

          x0_nextPage = x_center + x_offset_nextPage + matrix.height();
          y0_nextPage = y_center + y_offset_nextPage - matrix.height();
        }

        if (abs(max_offset_nextPage) == abs(max_offset_currentPage))
        {
          if (max_offset_nextPage == -max_offset_currentPage)
          {
            if (abs(currOffsetCurrentPage) <= abs(max_offset_currentPage))
            {
              x_offset_nextPage = 0;
              y_offset_nextPage = 0;
            }
            if (currOffsetNextPage == max_offset_nextPage)
            {
              x_offset_nextPage = 0;
              y_offset_nextPage = 0;
              animation = 0;
              currentPage++;
              if (currentPage > pageMax)
              {
                currentPage = 0;
              }
            }
          }
          else if (currOffsetNextPage == max_offset_nextPage)
          {
            x_offset_nextPage = 0;
            y_offset_nextPage = 0;
            animation = 0;
            currentPage++;
            if (currentPage > pageMax)
            {
              currentPage = 0;
            }
          }
        }
        else if (abs(max_offset_nextPage) > abs(max_offset_currentPage))
        {
          if (currOffsetNextPage == max_offset_nextPage)
          {
            x_offset_nextPage = 0;
            y_offset_nextPage = 0;
            animation = 0;
            currentPage++;
            if (currentPage > pageMax)
            {
              currentPage = 0;
            }
          }
        }
        else if (abs(max_offset_nextPage) < abs(max_offset_currentPage))
        {
          if (abs(currOffsetCurrentPage) <= abs(max_offset_currentPage) - abs(max_offset_nextPage))
          {
            x_offset_nextPage = 0;
            y_offset_nextPage = 0;
          }
          if (currOffsetNextPage == max_offset_nextPage)
          {
            x_offset_nextPage = 0;
            y_offset_nextPage = 0;
            animation = 0;
            currentPage++;
            if (currentPage > pageMax)
            {
              currentPage = 0;
            }
          }
        }
        else if (currOffsetNextPage == max_offset_nextPage)
        {
          x_offset_nextPage = 0;
          y_offset_nextPage = 0;
          animation = 0;
          currentPage++;
          if (currentPage > pageMax)
          {
            currentPage = 0;
          }
        }
        else
        {
          Serial.println(F("tidak ketangkap"));
        }

        matrix.setCursor(x0_nextPage, y0_nextPage);
        matrix.print(strNextPage);
      }

      //    //draw page indicator (rectangle)
      //    if (page == 0) {
      //        matrix.drawRect(28, 0, 2, 2, 1);
      //        matrix.drawLine(34, 1, 35, 1, 1);
      //    }
      //    else if (page == 1) {
      //        matrix.drawLine(28, 1, 29, 1, 1);
      //        matrix.drawRect(34, 0, 2, 2, 1);
      //    }
    }

    // -------------------------------------------------------------------
    // MODE 0, PAGE 7, SANDBOX
    // -------------------------------------------------------------------
    else if (currentPageMode0 == 7)
    {

      matrix.setTextWrap(0);

      int16_t x1Temp, y1Temp;
      uint16_t w1Temp, h1Temp;

      matrix.clearDisplay();

      matrix.setTextSize(1);

      const GFXfont *font;
      //font = &FiveBySeven5pt7b;
      font = &F14LED7pt8b;
      matrix.setFont(font);

      char buf[15];
      const char *ptr = PSTR("iqbaL");

      strcpy_P(buf, ptr);

      matrix.getTextBounds(buf, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);

      uint16_t x_start = (matrix.width() - w1Temp) / 2;
      uint16_t y_start = (matrix.height() - h1Temp) / 2 + h1Temp - 2;

      matrix.getTextBounds(buf, x_start, y_start, &x1Temp, &y1Temp, &w1Temp, &h1Temp);

      //    Serial.print(x1Temp);
      //    Serial.print(",");
      //    Serial.print(y1Temp);
      //    Serial.print(",");
      //    Serial.print(w1Temp);
      //    Serial.print(",");
      //    Serial.print(h1Temp);
      //    Serial.println();

      matrix.setCursor(x_start, y_start);
      matrix.print(buf);

      boolean effect0 = 1;

      if (effect0 == 1)
      {

        int16_t x0 = x1Temp;
        int16_t y0 = y1Temp;
        int16_t x1 = x1Temp;

        int16_t y1 = y1Temp;
        uint16_t w = w1Temp;
        uint16_t h = h1Temp;

        //      x0 = 0;
        //      x1 = 0;
        //      w = 48;

        static uint8_t animation = 0;

        uint16_t buf[w * h];

        matrix.copyBuffer(x0, y0, w, h, buf);

        //laser-in-left
        if (animation == 0)
        {

          static int16_t offset = 0;
          static uint16_t offsetMax;
          static int16_t xTemp = 0;
          uint16_t index = 0;

          matrix.fillRect(x0 + xTemp, y0, w, h, 0);

          offsetMax = w - xTemp;
          uint16_t sumBuf = 0;

          for (uint16_t yTemp = 0; yTemp < h; yTemp++)
          {
            matrix.drawPixel(x1 + w - offset, y1 + yTemp, buf[xTemp + index]);
            //detect empty spaces
            sumBuf = sumBuf + buf[xTemp + index];
            index = index + w;
          }

          //skip when found empty spaces
          if (sumBuf == 0)
          {
            xTemp++;
          }

          offset++;

          //reset offset and change animation
          if (offset > offsetMax)
          {
            offset = 0;
            xTemp++;
            if (xTemp == w)
            {
              xTemp = 0;
              if (animation == 0)
              {
                animation = 1;
              }
            }
          }
        }

        //laser-out-left
        else if (animation == 1)
        {

          static int16_t offset = 0;
          static uint16_t offsetMax;
          static int16_t xTemp = 0;
          uint16_t index = 0;

          matrix.fillRect(x0, y0, xTemp + 1, h, 0);

          offsetMax = xTemp;
          uint16_t sumBuf = 0;

          for (uint16_t yTemp = 0; yTemp < h; yTemp++)
          {

            matrix.drawPixel(x1 + xTemp - offset, y1 + yTemp, buf[xTemp + index]);
            sumBuf = sumBuf + buf[xTemp + index];
            index = index + w;
          }

          if (sumBuf == 0)
          {
            xTemp++;
          }

          offset++;

          //reset offset and change animation
          if (offset > offsetMax)
          {
            offset = 0;
            xTemp++;
            if (xTemp == w)
            {
              xTemp = 0;
              if (animation == 1)
              {
                animation = 0;
              }
            }
          }
        }
      }
    }

    // -------------------------------------------------------------------
    // MODE 0, PAGE 8, JAM / MENIT MENUJU SHOLAT TYPE 2
    // -------------------------------------------------------------------
    else if (currentPageMode0 == 8)
    {

      //if (!matrix.isReversed()) {
      //  matrix.reverse();
      //}
      matrix.setTextWrap(0);

      matrix.clearDisplay();

      matrix.setTextSize(1);

      const GFXfont *font;
      //font = &FiveBySeven5pt7b;
      font = &UbuntuMono_B10pt8b;
      //font = &F14LED7pt8b;
      matrix.setFont(font);

      int16_t xTemp, yTemp;
      uint16_t wTemp, hTemp;

      char contents[2] = "8";

      matrix.getTextBounds(contents, 0, 0, &xTemp, &yTemp, &wTemp, &hTemp);

      //prepare array for time left sholat
      char bufTimeLeftSholat[6];

      //format (add leading '0' if  less than 10) and store in the array
      sprintf(bufTimeLeftSholat, "%02d%02d%02d", HOUR, MINUTE, SECOND);

      int16_t x0 = 2;
      int16_t y0 = 13;

      if (bufTimeLeftSholat[1] != '0')
      {
        x0 = 2;
      }
      else if (bufTimeLeftSholat[1] == '0' && bufTimeLeftSholat[2] != '0')
      {
        x0 = 8;
      }
      else if (bufTimeLeftSholat[1] == '0' && bufTimeLeftSholat[2] == '0')
      {
        x0 = 13;
      }

      // print dash sign
      int dashLength = 7;
      if (state500ms)
      {
        matrix.fillRect(x0, 7, dashLength, 2, 1);
      }

      x0 = x0 + dashLength + 1;

      // print hour digit2
      if (bufTimeLeftSholat[1] != '0')
      {
        matrix.setCursor(x0, y0);
        matrix.print(bufTimeLeftSholat[1]);

        //print Colon
        x0 = x0 + wTemp + 2;
        matrix.fillRect(x0, y0 - 3, 2, 2, 1);
        matrix.fillRect(x0, y0 - 9, 2, 2, 1);
        x0 = x0 + 2;
      }

      //print minute digit 1
      if (bufTimeLeftSholat[2] != '0' || (bufTimeLeftSholat[2] == '0' && bufTimeLeftSholat[1] != '0'))
      {
        matrix.setCursor(x0, y0);
        matrix.print(bufTimeLeftSholat[2]);
        x0 = x0 + wTemp + 1;
      }

      // print minute digit 2
      matrix.setCursor(x0, y0);
      matrix.print(bufTimeLeftSholat[3]);

      //print Colon
      x0 = x0 + wTemp + 2;
      matrix.fillRect(x0, y0 - 3, 2, 2, 1);
      matrix.fillRect(x0, y0 - 9, 2, 2, 1);

      // print second digit 1
      x0 = x0 + 2;
      matrix.setCursor(x0, y0);
      matrix.print(bufTimeLeftSholat[4]);

      // print second digit 2
      x0 = x0 + wTemp + 1;
      matrix.setCursor(x0, y0);
      matrix.print(bufTimeLeftSholat[5]);
    }

    // -------------------------------------------------------------------
    // MODE 0, PAGE 9, JAM SAJA TYPE 2
    // -------------------------------------------------------------------
    else if (currentPageMode0 == 9)
    {
      byte typePage9 = 1;

      if (typePage9 == 0)
      {
        matrix.setTextWrap(0);

        matrix.clearDisplay();

        matrix.setTextSize(1);

        const GFXfont *font;
        //font = &FiveBySeven5pt7b;
        font = &UbuntuMono_B10pt8b;
        //font = &F14LED7pt8b;
        matrix.setFont(font);

        // calculate font width
        int16_t xTemp, yTemp;
        uint16_t wTemp, hTemp;
        char contents[2] = "8";
        matrix.getTextBounds(contents, 0, 0, &xTemp, &yTemp, &wTemp, &hTemp);

        //break time element for NOW time
        time_t t = local_time();
        tmElements_t tmSet;
        breakTime(t, tmSet);

        //array for time Now
        char bufNow[6];

        //format (add leading '0' if  less than 10) and store in the array
        sprintf(bufNow, "%02d%02d%02d", hourFormat12(t), tmSet.Minute, tmSet.Second);

        int16_t x0;
        int16_t y0;

        // set starting row row based on font type
        if (font == &UbuntuMono_B10pt8b)
        {
          y0 = 13;
        }

        //determine time Format 12-hour or 24-hour
        // 0 = 24-hour
        // 1 = 12-hour
        byte timeFormat = 1;

        // determine starting column
        if (bufNow[0] != '0')
        {
          if (timeFormat == 0)
          {
            x0 = 0;
          }
          else if (timeFormat == 1)
          {
            x0 = -1;
          }
        }
        else if (bufNow[0] == '0')
        {
          x0 = 4;
        }

        // print hour digit 1
        if (bufNow[0] != '0')
        {
          matrix.setCursor(x0, y0);
          matrix.print(bufNow[0]);
          x0 = x0 + wTemp + 1;
        }

        // print hour digit2
        matrix.setCursor(x0, y0);
        matrix.print(bufNow[1]);

        //print Colon
        x0 = x0 + wTemp + 2;
        matrix.fillRect(x0, y0 - 2, 4, 2, 1);
        matrix.fillRect(x0 + 1, y0 - 3, 2, 4, 1);
        matrix.fillRect(x0, y0 - 8, 4, 2, 1);
        matrix.fillRect(x0 + 1, y0 - 9, 2, 4, 1);

        //print minute digit 1
        x0 = x0 + 4;
        matrix.setCursor(x0, y0);
        matrix.print(bufNow[2]);
        //matrix.print(":");
        x0 = x0 + wTemp + 1;

        // print minute digit 2
        matrix.setCursor(x0, y0);
        matrix.print(bufNow[3]);

        //print Colon
        x0 = x0 + wTemp + 2;
        matrix.fillRect(x0, y0 - 2, 4, 2, 1);
        matrix.fillRect(x0 + 1, y0 - 3, 2, 4, 1);
        matrix.fillRect(x0, y0 - 8, 4, 2, 1);
        matrix.fillRect(x0 + 1, y0 - 9, 2, 4, 1);

        // print second digit 1
        x0 = x0 + 4;
        matrix.setCursor(x0, y0);
        matrix.print(bufNow[4]);

        // print second digit 2
        x0 = x0 + wTemp + 1;
        matrix.setCursor(x0, y0);
        matrix.print(bufNow[5]);
      }

      else if (typePage9 == 1)
      {
        matrix.setTextWrap(0);

        matrix.clearDisplay();

        matrix.setTextSize(1);

        const GFXfont *font;
        //font = &FiveBySeven5pt7b;
        font = &UbuntuMono_B10pt8b;
        //font = &F14LED7pt8b;
        matrix.setFont(font);

        // calculate font width
        int16_t xTemp, yTemp;
        uint16_t wTemp, hTemp;
        char contents[2] = "8";
        matrix.getTextBounds(contents, 0, 0, &xTemp, &yTemp, &wTemp, &hTemp);

        //break time element for NOW time
        time_t t = local_time();
        tmElements_t tmSet;
        breakTime(t, tmSet);

        //array for time Now
        char bufNow[6];

        //format (add leading '0' if  less than 10) and store in the array
        sprintf(bufNow, "%02d%02d%02d", hourFormat12(t), tmSet.Minute, tmSet.Second);

        int16_t x0;
        int16_t y0;

        // set starting row row based on font type
        if (font == &UbuntuMono_B10pt8b)
        {
          y0 = 13;
        }

        //determine time Format 12-hour or 24-hour
        // 0 = 24-hour
        // 1 = 12-hour
        byte timeFormat = 1;

        // determine starting column
        if (bufNow[0] != '0')
        {
          if (timeFormat == 0)
          {
            x0 = 0;
          }
          else if (timeFormat == 1)
          {
            x0 = -1;
          }
        }
        else if (bufNow[0] == '0')
        {
          x0 = 4;
        }

        int x0_hour_digit_1 = x0;

        // print hour digit 1
        if (bufNow[0] != '0')
        {
          matrix.setCursor(x0, y0);
          matrix.print(bufNow[0]);
          x0 = x0 + wTemp + 1;
        }

        int x0_hour_digit_2 = x0;

        // print hour digit2
        matrix.setCursor(x0, y0);
        matrix.print(bufNow[1]);

        //print Colon
        x0 = x0 + wTemp + 2;
        matrix.fillRect(x0, y0 - 2, 4, 2, 1);
        matrix.fillRect(x0 + 1, y0 - 3, 2, 4, 1);
        matrix.fillRect(x0, y0 - 8, 4, 2, 1);
        matrix.fillRect(x0 + 1, y0 - 9, 2, 4, 1);

        //print minute digit 1
        x0 = x0 + 4;
        int x0_minute_digit_1 = x0;
        matrix.setCursor(x0, y0);
        matrix.print(bufNow[2]);
        //matrix.print(":");
        x0 = x0 + wTemp + 1;
        int x0_minute_digit_2 = x0;

        // print minute digit 2
        matrix.setCursor(x0, y0);
        matrix.print(bufNow[3]);

        //print Colon
        x0 = x0 + wTemp + 2;
        matrix.fillRect(x0, y0 - 2, 4, 2, 1);
        matrix.fillRect(x0 + 1, y0 - 3, 2, 4, 1);
        matrix.fillRect(x0, y0 - 8, 4, 2, 1);
        matrix.fillRect(x0 + 1, y0 - 9, 2, 4, 1);

        // print second digit 1
        x0 = x0 + 4;
        int x0_second_digit_1 = x0;
        matrix.setCursor(x0, y0);
        matrix.print(bufNow[4]);

        // print second digit 2
        x0 = x0 + wTemp + 1;
        int x0_second_digit_2 = x0;
        matrix.setCursor(x0, y0);
        matrix.print(bufNow[5]);

        //array for 1 second ahead
        static char bufTimeFuture[6];

        //counter to determine when to trigger the animation
        int sec;
        static int second_old = 0;
        sec = bufNow[5];
        // static unsigned long currMillis;
        static uint16_t count = 0;
        if (sec != second_old)
        {
          second_old = sec;
          // currMillis = millis();
          count = 0;
        }

        static boolean animateSecDigit1;
        static boolean animateSecDigit2;
        static boolean animateMinDigit1;
        static boolean animateMinDigit2;
        static boolean animateHourDigit1;
        static boolean animateHourDigit2;

        if (count == 1000 / _ledMatrixSettings.scrollspeed - 25)
        {
          //break time element
          time_t t_future = local_time() + 1;
          tmElements_t tmSet_future;
          breakTime(t_future, tmSet_future);

          //format (add leading '0' if less than 10) and store in the array
          sprintf(bufTimeFuture, "%02d%02d%02d", tmSet_future.Hour, tmSet_future.Minute, tmSet_future.Second);

          //animateSecDigit2 = true;

          if (bufNow[5] == '9')
          {
            //animateSecDigit1 = true;
            if (bufNow[4] == '5')
            {
              animateMinDigit2 = true;
              if (bufNow[3] == '9')
              {
                animateMinDigit1 = true;
                if (bufNow[2] == '5')
                {
                  animateHourDigit2 = true;
                  if (bufNow[1] == '3' || bufNow[1] == '9')
                  {
                    animateHourDigit1 = true;
                  }
                }
              }
            }
          }
        }

        if (count == 1000 / _ledMatrixSettings.scrollspeed - 25)
        {

          animateSecDigit2 = true;

          if (bufNow[5] == '9')
          {
            animateSecDigit1 = true;
          }
        }

        count++;

        uint16_t w = 9;
        uint16_t h = 12;

        if (animateSecDigit2)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_sec_digit_2(bufNow[5], font, x0_second_digit_2, y0, w, h);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_sec_digit_2(bufTimeFuture[5], font, x0_second_digit_2, y0, w, h);
            if (slideInDown == true)
            {
              animateSecDigit2 = false;
              animation = 0;
            }
          }
        }

        if (animateSecDigit1)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_sec_digit_1(bufNow[4], font, x0_second_digit_1, y0, w, h);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_sec_digit_1(bufTimeFuture[4], font, x0_second_digit_1, y0, w, h);
            if (slideInDown == true)
            {
              animateSecDigit1 = false;
              animation = 0;
            }
          }
        }

        if (animateMinDigit2)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_min_digit_2(bufNow[3], font, x0_minute_digit_2, y0, w, h);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_min_digit_2(bufTimeFuture[3], font, x0_minute_digit_2, y0, w, h);
            if (slideInDown == true)
            {
              animateMinDigit2 = false;
              animation = 0;
            }
          }
        }

        if (animateMinDigit1)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_min_digit_1(bufNow[2], font, x0_minute_digit_1, y0, w, h);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_min_digit_1(bufTimeFuture[2], font, x0_minute_digit_1, y0, w, h);
            if (slideInDown == true)
            {
              animateMinDigit1 = false;
              animation = 0;
            }
          }
        }

        if (animateHourDigit2)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_hour_digit_2(bufNow[1], font, x0_hour_digit_2, y0, w, h);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_hour_digit_2(bufTimeFuture[1], font, x0_hour_digit_2, y0, w, h);
            if (slideInDown == true)
            {
              animateHourDigit2 = false;
              animation = 0;
            }
          }
        }

        if (animateHourDigit1)
        {
          static int animation = 0;

          //slide-out-down
          if (animation == 0)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideOutDown = slide_out_down_hour_digit_1(bufNow[0], font, x0_hour_digit_1, y0, w, h);
            if (slideOutDown == true)
            {
              animation = 1;
            }
          }

          //slide-in-down
          if (animation == 1)
          {
            //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
            boolean slideInDown = slide_in_down_hour_digit_1(bufTimeFuture[0], font, x0_hour_digit_1, y0, w, h);
            if (slideInDown == true)
            {
              animateHourDigit1 = false;
              animation = 0;
            }
          }
        }
      }
    }

    // -------------------------------------------------------------------
    // MODE 0, PAGE 10, SANDBOX 2
    // -------------------------------------------------------------------
    else if (currentPageMode0 == 10)
    {

      static int c;

      matrix.clearDisplay();

      matrix.setTextSize(1);

      const GFXfont *font = &F14LED7pt8b;
      matrix.setFont(font);

      if (font == &TomThumb)
      {
        matrix.setCursor(c, 14);
      }
      else if (font == &bold_led_board_7_regular4pt7b)
      {
        matrix.setCursor(c, 12);
      }
      else if (font == &ChessType9pt7b)
      {
        matrix.setCursor(c, 12);
      }
      else if (font == &FiveBySeven5pt7b)
      {
        matrix.setCursor(c, 14);
      }
      else if (font == &F14LED7pt8b)
      {
        matrix.setCursor(c, 12);
      }

      //ESPHTTPServer.load_running_text();
      matrix.print("A");

      static int count;

      if (count == 80)
      {
        count = 0;
        c++;
      }
      count++;

      if (c > 63)
      {
        c = 0;
      }
    }

    // -------------------------------------------------------------------
    // MODE 0, PAGE 11, IQAMAH TIME
    // -------------------------------------------------------------------
    else if (currentPageMode0 == 11)
    {
      //calculate time to iqamah
      time_t adzanEndTime = currentSholatTime + (60 * _ledMatrixSettings.adzanwaittime);
      time_t iqamahTime = adzanEndTime + (60 * _ledMatrixSettings.iqamahwaittime);

      // time_t iqamahTime = currentSholatTime + (60 * 7);

      time_t timeToIqamah = iqamahTime - localTime;

      // uint16_t days;
      uint8_t hours;
      uint8_t minutes;
      uint8_t seconds;

      // days = elapsedDays(iqamahTime);
      hours = numberOfHours(timeToIqamah);
      minutes = numberOfMinutes(timeToIqamah);
      seconds = numberOfSeconds(timeToIqamah);

      matrix.setTextWrap(0);

      matrix.clearDisplay();

      matrix.setTextSize(1);

      const GFXfont *font;
      //font = &FiveBySeven5pt7b;
      font = &UbuntuMono_B10pt8b;
      //font = &F14LED7pt8b;
      matrix.setFont(font);

      int16_t xTemp, yTemp;
      uint16_t wTemp, hTemp;

      char contents[2] = "8";

      matrix.getTextBounds(contents, 0, 0, &xTemp, &yTemp, &wTemp, &hTemp);

      //prepare array for time left sholat
      char bufTimeToIqamah[7];

      //format (add leading '0' if  less than 10) and store in the array
      sprintf(bufTimeToIqamah, "%02d%02d%02d", hours, minutes, seconds);

      int16_t x0 = 2;
      int16_t y0 = 13;

      if (bufTimeToIqamah[1] != '0')
      {
        x0 = 2;
      }
      else if (bufTimeToIqamah[1] == '0' && bufTimeToIqamah[2] != '0')
      {
        x0 = 8;
      }
      else if (bufTimeToIqamah[1] == '0' && bufTimeToIqamah[2] == '0')
      {
        x0 = 13;
      }

      // print dash sign
      int dashLength = 7;
      if (state500ms)
      {
        matrix.fillRect(x0, 7, dashLength, 2, 1);
      }

      x0 = x0 + dashLength + 1;

      // print hour digit2
      if (bufTimeToIqamah[1] != '0')
      {
        matrix.setCursor(x0, y0);
        matrix.print(bufTimeToIqamah[1]);

        //print Colon
        x0 = x0 + wTemp + 2;
        matrix.fillRect(x0, y0 - 3, 2, 2, 1);
        matrix.fillRect(x0, y0 - 9, 2, 2, 1);
        x0 = x0 + 2;
      }

      //print minute digit 1
      if (bufTimeToIqamah[2] != '0' || (bufTimeToIqamah[2] == '0' && bufTimeToIqamah[1] != '0'))
      {
        matrix.setCursor(x0, y0);
        matrix.print(bufTimeToIqamah[2]);
        x0 = x0 + wTemp + 1;
      }

      // print minute digit 2
      matrix.setCursor(x0, y0);
      matrix.print(bufTimeToIqamah[3]);

      //print Colon
      x0 = x0 + wTemp + 2;
      matrix.fillRect(x0, y0 - 3, 2, 2, 1);
      matrix.fillRect(x0, y0 - 9, 2, 2, 1);

      // print second digit 1
      x0 = x0 + 2;
      matrix.setCursor(x0, y0);
      matrix.print(bufTimeToIqamah[4]);

      // print second digit 2
      x0 = x0 + wTemp + 1;
      matrix.setCursor(x0, y0);
      matrix.print(bufTimeToIqamah[5]);
    }

    // -------------------------------------------------------------------
    // MODE 0, PAGE 12, JAM DAN TANGGAL TYPE 2
    // -------------------------------------------------------------------
    else if (currentPageMode0 == 12)
    {

      matrix.clearDisplay();

      matrix.setTextWrap(0);

      int c;
      int row0 = 6;
      // int row1 = 15;
      int startCol = 1;
      int x0 = X;
      // int x1 = X;

      if (_ledMatrixSettings.scrollrow_0 == false)
      {
        x0 = 0;
      }

      if (_ledMatrixSettings.scrollrow_1 == false)
      {
        // x1 = 0;
      }

      // -------------------------------------------------------------------
      // ROW 0
      // -------------------------------------------------------------------

      startCol = 15; //override start colum for Row0

      const GFXfont *fontRow0 = &Wide11x8;
      matrix.setFont(fontRow0);

      if (fontRow0 == &Wide11x8)
      {
        startCol = 9;
        row0 = 7;
      }
      else if (fontRow0 == &bold_led_board_7_regular4pt7b)
      {
        startCol = 15;
        row0 = 6;
      }

      //hour
      c = DISP_REFF + startCol + x0;
      matrix.setCursor(c, row0);
      
      char buf[2];
      sprintf(buf, "%02d", hour(localTime));
      matrix.print(buf);

      //colon
      c = matrix.getCursorX() + 1;

      if (second() < 30)
      {
        matrix.fillRect(c, row0 - 1, 2, 2, 1);

        // if (blinkColon) {
        if (state1000ms)
        {
          matrix.fillRect(c, row0 - 4, 2, 2, 1);
        }
        // }
      }
      else
      {
        matrix.fillRect(c, row0 - 4, 2, 2, 1);

        // if (blinkColon) {
        if (state1000ms)
        {
          matrix.fillRect(c, row0 - 1, 2, 2, 1);
        }
        // }
      }

      //minute
      c = matrix.getCursorX() + 5;
      matrix.setCursor(c, row0);

      sprintf(buf, "%02d", minute(localTime));
      matrix.print(buf);

      // -------------------------------------------------------------------
      // ROW 1
      // -------------------------------------------------------------------
      matrix.setTextWrap(0);

      // int16_t x1Temp, y1Temp;
      // uint16_t wTemp, hTemp;

      // int c;
      // int row0 = 6;
      // int row1 = 14;
      startCol = 1;

      if (_ledMatrixSettings.scrollrow_0 == false)
      {
        c = DISP_REFF + startCol;
      }

      if (_ledMatrixSettings.scrollrow_1 == false)
      {
        c = DISP_REFF + startCol;
      }

      c = DISP_REFF + startCol + X;

      // matrix.clearDisplay();

      matrix.setTextSize(1);

      const GFXfont *font = &FiveBySeven5pt7b;
      matrix.setFont(font);

      if (font == &TomThumb)
      {
        matrix.setCursor(c, 14);
      }
      else if (font == &bold_led_board_7_regular4pt7b)
      {
        matrix.setCursor(c, 12);
      }
      else if (font == &ChessType9pt7b)
      {
        matrix.setCursor(c, 12);
      }
      else if (font == &RepetitionScrolling5pt8b)
      {
        matrix.setCursor(c, 14);
      }
      else if (font == &FiveBySeven5pt7b)
      {
        matrix.setCursor(c, 15);
      }
      else if (font == &F14LED7pt8b)
      {
        matrix.setCursor(c, 12);
      }

      load_running_text_2();

      // -------------------------------------------------------------------
      // Animation
      // -------------------------------------------------------------------
      boolean effect0 = 0;
      // boolean effect1 = 0;

      if (effect0 == 1)
      {
        static uint16_t scan = 0;
        //uint16_t scanMax = _ledMatrixSettings.scrollspeed / 7;
        uint16_t scanMax = _ledMatrixSettings.scrollspeed;
        if (scan < scanMax)
        {
          scan++;
        }
        else if (scan >= scanMax)
        {
          scan = 0;
        }

        //    int16_t x0 = 2;
        //    int16_t y0 = 3;
        //    int16_t x1 = 2;
        //    int16_t y1 = 3;
        //    uint16_t w = 14;
        //    uint16_t h = 10;
      }

      if (effect0 == 1)
      {
        int16_t x0 = 0;
        int16_t y0 = 0;
        int16_t x1 = 0;
        int16_t y1 = 0;
        uint16_t w = 64;
        uint16_t h = 16;

        static int16_t offset;
        static uint16_t offsetMax;

        static uint8_t animation = 6;

        uint16_t index;
        uint16_t buf[1024];

        matrix.copyBuffer(x0, y0, w, h, buf);
        matrix.fillRect(x0, y0, w, h, 0);

        for (uint16_t yTemp = 0; yTemp < h; yTemp++)
        {
          for (uint16_t xTemp = 0; xTemp < w; xTemp++)
          {

            //slide-out-up
            if (animation == 0)
            {
              offsetMax = h;
              if (yTemp >= offset)
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - offset, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - offset, 0);
              }
            }

            //slide-out-down
            else if (animation == 1)
            {
              offsetMax = h;
              if (yTemp < h - offset)
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, 0);
              }
            }

            //slide-out-right
            else if (animation == 2)
            {
              offsetMax = w;
              if (xTemp < w - offset)
              {
                matrix.drawPixel(x1 + xTemp + offset, y1 + yTemp, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp + offset, y1 + yTemp, 0);
              }
            }

            //slide-out-to-left
            else if (animation == 3)
            {
              offsetMax = w;
              if (xTemp >= offset)
              {
                matrix.drawPixel(x1 + xTemp - offset, y1 + yTemp, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp - offset, y1 + yTemp, 0);
              }
            }

            //slide-in-right
            else if (animation == 4)
            {
              offsetMax = w;
              if (xTemp >= w - offset)
              {
                matrix.drawPixel(x1 + xTemp - w + offset, y1 + yTemp, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp - w + offset, y1 + yTemp, 0);
              }
            }

            //slide-in-up
            else if (animation == 5)
            {
              offsetMax = h;
              if (yTemp < offset)
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + h - offset, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + h - offset, 0);
              }
            }

            //slide-in-down
            else if (animation == 6)
            {
              offsetMax = h;
              if (yTemp >= h - offset)
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
              }
            }

            //slide-in-left
            else if (animation == 7)
            {
              offsetMax = w;
              if (xTemp < offset)
              {
                matrix.drawPixel(x1 + xTemp + w - offset, y1 + yTemp, buf[index]);
              }
              else
              {
                matrix.drawPixel(x1 + xTemp + w - offset, y1 + yTemp, 0);
              }
            }

            index++;
          }
        }

        offset++;

        //reset offset and change animation
        if (offset > offsetMax)
        {
          offset = 0;
          if (animation == 6)
          {
            animation = 3;
          }
          else if (animation == 3)
          {
            animation = 4;
          }
          else if (animation == 4)
          {
            animation = 1;
          }
          else if (animation == 1)
          {
            animation = 5;
          }
          else if (animation == 5)
          {
            animation = 2;
          }
          else if (animation == 2)
          {
            animation = 7;
          }
          else if (animation == 7)
          {
            animation = 0;
          }
          else if (animation == 0)
          {
            animation = 6;
          }
        }
      }
    }
  }

  // ------------
  // MODE 1
  // ------------
  else if (MODE == 1)
  {
    if (MODE != MODE_old)
    {
      MODE_old = MODE;
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.println(F("MODE 1 [Config Mode]"));
    }

    // static bool refresh;
    if (currentPageMode1_old != currentPageMode1)
    {
      currentPageMode1_old = currentPageMode1;
      digitalClockDisplay();
      Serial.print(F("> CONFIG MODE, PAGE "));
      Serial.println(currentPageMode1);

      // refresh = true;
      matrix.clearDisplay();
    }

    // -------------------------------------------------------------------
    // MODE 1, PAGE 0
    // DEVICE NAME
    // -------------------------------------------------------------------
    if (currentPageMode1 == 0)
    {

      //clear Screen
      matrix.clearDisplay();

      // ------------------------
      // String "Name" at row 0
      // ------------------------

      char str[64];

      const char *ptr = PSTR("Host Name");

      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      uint16_t x1 = (matrix.width() - w) / 2 + 4;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // ------------------------
      // Device name at row 1
      // ------------------------

      strlcpy(str, WiFi.hostname().c_str(), sizeof(str));

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      x1 = (matrix.width() - w) / 2 + 4;
      y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);
    }

    // -------------------------------------------------------------------
    // MODE 1, PAGE 1
    // SSID
    // -------------------------------------------------------------------
    if (currentPageMode1 == 1)
    {

      //clear Screen
      matrix.clearDisplay();

      // ------------------------
      // String "SSID" at row 0
      // ------------------------

      char str[64];

      const char *ptr = PSTR("SSID");

      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      uint16_t x1 = (matrix.width() - w) / 2 + 4;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // ------------------------
      // SSID name at row 1
      // ------------------------

      strlcpy(str, WiFi.SSID().c_str(), sizeof(str));

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      x1 = (matrix.width() - w) / 2 + 4;
      y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2 - 1;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);
    }

    // -------------------------------------------------------------------
    // MODE 1, PAGE 2
    // IP ADDRESS
    // -------------------------------------------------------------------
    if (currentPageMode1 == 2)
    {

      //clear Screen
      matrix.clearDisplay();

      // ------------------------
      // String "IP Address" at row 0
      // ------------------------

      char str[64];
      const char *ptr = PSTR("IP Address");
      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      uint16_t x1 = (matrix.width() - w) / 2 + 4;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // ------------------------
      // Local IP Address at row 1
      // ------------------------

      strlcpy(str, WiFi.localIP().toString().c_str(), sizeof(str));

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      x1 = (matrix.width() - w) / 2 + 4;
      y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);
    }

    // -------------------------------------------------------------------
    // MODE 1, PAGE 3
    // GATEWAY
    // -------------------------------------------------------------------
    if (currentPageMode1 == 3)
    {

      //clear Screen
      matrix.clearDisplay();

      // ------------------------
      // String "Gateway" at row 0
      // ------------------------

      char str[64];
      const char *ptr = PSTR("Gateway");
      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      uint16_t x1 = (matrix.width() - w) / 2 + 4;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h - 1;

      //update x1 and y1 value
      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // ------------------------
      // Gateway IP Address at row 1
      // ------------------------

      strlcpy(str, WiFi.gatewayIP().toString().c_str(), sizeof(str));

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      x1 = (matrix.width() - w) / 2 + 4;
      y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

      //update x1 and y1 value
      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);
    }

    // -------------------------------------------------------------------
    // MODE 1, PAGE 4
    // NETMASK
    // -------------------------------------------------------------------
    if (currentPageMode1 == 4)
    {

      //clear Screen
      matrix.clearDisplay();

      // ------------------------
      // String "Netmask" at row 0
      // ------------------------

      char str[64];
      const char *ptr = PSTR("Netmask");
      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      uint16_t x1 = (matrix.width() - w) / 2 + 4;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // ------------------------
      // Netmask Address at row 1
      // ------------------------

      strlcpy(str, WiFi.subnetMask().toString().c_str(), sizeof(str));

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      x1 = (matrix.width() - w) / 2 + 4;
      y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);
    }

    // -------------------------------------------------------------------
    // MODE 1, PAGE 5
    // MAC ADDRESS
    // -------------------------------------------------------------------
    if (currentPageMode1 == 5)
    {

      //clear Screen
      matrix.clearDisplay();

      // ------------------------
      // String "MAC Address" at row 0
      // ------------------------

      char str[64];
      const char *ptr = PSTR("MAC Address");
      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      uint16_t x1 = (matrix.width() - w) / 2 + 4;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // ------------------------
      // MAC Address at row 1
      // ------------------------

      strlcpy(str, WiFi.macAddress().c_str(), sizeof(str));

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      x1 = (matrix.width() - w) / 2 + 4;
      y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);
    }

    // -------------------------------------------------------------------
    // MODE 1, PAGE 6
    // DNS ADDRESS
    // -------------------------------------------------------------------
    if (currentPageMode1 == 6)
    {

      //clear Screen
      matrix.clearDisplay();

      // ------------------------
      // String "DNS" at row 0
      // ------------------------

      char str[64];
      const char *ptr = PSTR("DNS");
      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      uint16_t x1 = (matrix.width() - w) / 2 + 4;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // ------------------------
      // DNS Address at row 1
      // ------------------------

      strlcpy(str, WiFi.dnsIP().toString().c_str(), sizeof(str));

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      x1 = (matrix.width() - w) / 2 + 4;
      y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);
    }

    // -------------------------------------------------------------------
    // MODE 1, PAGE 7
    // LAST SYNC
    // -------------------------------------------------------------------
    if (currentPageMode1 == 7)
    {

      //clear Screen
      matrix.clearDisplay();

      // ------------------------
      // String "Last sync" at row 0
      // ------------------------

      char str[64];
      const char *ptr = PSTR("Last sync");
      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      uint16_t x1 = (matrix.width() - w) / 2 + 4;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h - 1;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // ------------------------
      // Last sync time at row 1
      // ------------------------

      strcpy(str, getDateTimeString(local_time(_lastSyncd)));

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      x1 = (matrix.width() - w) / 2 + 4;
      y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);
    }

    // -------------------------------------------------------------------
    // MODE 1, PAGE 8
    // TIME
    // -------------------------------------------------------------------
    if (currentPageMode1 == 8)
    {

      //clear Screen
      matrix.clearDisplay();

      // ------------------------
      // String "Time" at row 0
      // ------------------------

      char str[64];
      const char *ptr = PSTR("Time");
      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      uint16_t x1 = (matrix.width() - w) / 2 + 4;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // ------------------------
      // Local time at row 1
      // ------------------------

      strlcpy(str, getTimeStr(local_time()), sizeof(str));

      matrix.setFont(&bold_led_board_7_regular4pt7b);
      matrix.setFont(&FiveBySeven5pt7b);
      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      x1 = (matrix.width() - w) / 2 + 4;
      y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);
    }

    // -------------------------------------------------------------------
    // MODE 1, PAGE 9
    // DATE
    // -------------------------------------------------------------------
    if (currentPageMode1 == 9)
    {

      //clear Screen
      matrix.clearDisplay();

      // ------------------------
      // String "Date" at row 0
      // ------------------------

      char str[64];
      const char *ptr = PSTR("Date");
      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      uint16_t x1 = (matrix.width() - w) / 2 + 4;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // ------------------------
      // Local date at row 1
      // ------------------------

      strlcpy(str, getDateStr(local_time()), sizeof(str));

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      x1 = (matrix.width() - w) / 2 + 4;
      y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);
    }

    // -------------------------------------------------------------------
    // MODE 1, PAGE 10
    // UPTIME
    // -------------------------------------------------------------------
    if (currentPageMode1 == 10)
    {

      //clear Screen
      matrix.clearDisplay();

      // ------------------------
      // String "Uptime" at row 0
      // ------------------------

      char str[64];
      const char *ptr = PSTR("Uptime");
      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      uint16_t x1 = (matrix.width() - w) / 2 + 4;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h - 1;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // ------------------------
      // Uptime at row 1
      // ------------------------

      strlcpy(str, getUptimeStr(), sizeof(str));

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      x1 = (matrix.width() - w) / 2 + 4;
      y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);
    }

    // -------------------------------------------------------------------
    // MODE 1, PAGE 11
    // LAST BOOT
    // -------------------------------------------------------------------
    if (currentPageMode1 == 11)
    {

      //clear Screen
      matrix.clearDisplay();

      // ------------------------
      // String "Last boot" at row 0
      // ------------------------

      char str[64];
      const char *ptr = PSTR("Last boot");
      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      uint16_t x1 = (matrix.width() - w) / 2 + 4;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // ------------------------
      // Last boot time at row 1
      // ------------------------

      // strlcpy(str, getDateTimeString(local_time(_lastBoot)), sizeof(str));
      strlcpy(str, getLastBootStr(), sizeof(str));

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      x1 = (matrix.width() - w) / 2 + 4;
      y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);
    }

    // -------------------------------------------------------------------
    // MODE 1, PAGE 12
    // FREE HEAP
    // -------------------------------------------------------------------
    if (currentPageMode1 == 12)
    {

      //clear Screen
      matrix.clearDisplay();

      // ------------------------
      // String "Free heap" at row 0
      // ------------------------

      char str[64];
      const char *ptr = PSTR("Free heap");
      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      uint16_t x1 = (matrix.width() - w) / 2 + 4;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h - 1;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // ------------------------
      // Free heap at row 1
      // ------------------------

      itoa(ESP.getFreeHeap(), str, 10);

      matrix.setFont(&TomThumb);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

      x1 = (matrix.width() - w) / 2 + 4;
      y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);
    }
  }

  // -------------------------------------------------------------------
  // MODE 2
  // -------------------------------------------------------------------

  else if (MODE == 2)
  {
    static bool stateEdit = false;
    static uint32_t startMillis = 0;
    static bool stateSwitch_old = false;
    static int16_t count = 0;
    static int16_t pos = -1;
    static int16_t encoder0Pos_old = encoder0Pos;

    if (MODE != MODE_old)
    {
      MODE_old = MODE;
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.println(F("MODE 2 [Edit Mode]"));

      // reset static variables
      stateEdit = false;
      startMillis = millis();
      stateSwitch_old = false;
      count = 0;
      pos = -1;
      encoder0Pos_old = encoder0Pos;
    }

    // static uint32_t timerSwitch;

    if (stateSwitch != stateSwitch_old)
    {

      stateSwitch_old = stateSwitch;

      if (stateSwitch == HIGH)
      {
        // startMillis = millis();

        // stateEdit = !stateEdit;

        // timerSwitch = millis();
        // alarmState = HIGH;
        // tone0 = HIGH;

        // if (stateEdit == false)
        // {
        //   _config.timezone = tempTimezone;
        //   save_config();
        // }
      }
      else if (stateSwitch == LOW)
      {
        // stateEdit = false;
      }
    }

    bool encoder0PosIncreasing = false;
    bool encoder0PosDecreasing = false;

    if (encoder0Pos > encoder0Pos_old)
    {
      encoder0PosIncreasing = true;
      encoder0Pos_old = encoder0Pos;
    }
    else if (encoder0Pos < encoder0Pos_old)
    {
      encoder0PosDecreasing = true;
      encoder0Pos_old = encoder0Pos;
    }

    if (encoder0PosIncreasing && stateEdit == false)
    {
      currentPageMode2++;
      if (currentPageMode2 >= PageTitleMode2Count)
      {
        currentPageMode2 = 0;
      }
    }
    else if (encoder0PosDecreasing && stateEdit == false)
    {
      currentPageMode2--;
      if (currentPageMode2 <= 0 || currentPageMode2 >= PageTitleMode2Count)
      {
        currentPageMode2 = PageTitleMode2Count - 1;
      }
    }

    // -------------------------------------------------------------------
    // MODE 2, PAGE 0
    // SETTING TIMEZONE
    // -------------------------------------------------------------------
    if (currentPageMode2 == 0)
    {
      // static int8_t tempTimezone = 0;

      if (currentPageMode2_old != currentPageMode2)
      {
        currentPageMode2_old = currentPageMode2;
        digitalClockDisplay();
        Serial.print(F("> EDIT MODE, PAGE "));
        Serial.println(currentPageMode2);

        // reset static variables used in this page
        stateEdit = false;
        stateSwitch_old = false;
        count = 0;
        // tempTimezone = _config.timezone;
      }

      //clear Screen
      matrix.clearDisplay();

      // -------
      // Row 0
      // -------

      char str[16];
      const char *ptr = PSTR("Set Timezone");
      strcpy_P(str, ptr);

      int16_t x0, y0;
      uint16_t w, h;
      matrix.setFont(&TomThumb);
      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);
      uint16_t x1 = (matrix.width() - (w - 4)) / 2;
      uint16_t y1 = (matrix.height() / 2 - h) / 2 + h - 0;
      //update x1 and y1 value
      matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      //Print to led matrix
      matrix.setCursor(x1, y1);
      matrix.print(str);

      // -------
      // Row 1
      // -------

      // matrix.setFont(&TomThumb);
      // matrix.setFont(&bold_led_board_7_regular4pt7b);
      // matrix.setFont(&RepetitionScrolling5pt8b);
      matrix.setFont(&FiveBySeven5pt7b);
      // matrix.setFont(&Org_01);

      if (stateEdit)
      {
        if (encoder0PosIncreasing)
        {
          count++;
          if (count == timezoneCount)
          {
            count = 0;
          }
        }
        else if (encoder0PosDecreasing)
        {
          count--;
          if (count < 0)
          {
            count = timezoneCount - 1;
          }
        }

        if (count == WIB)
        {
          ptr = PSTR("WIB (+7)");
          // tempTimezone = 70;
        }
        if (count == WITA)
        {
          ptr = PSTR("WITA (+8)");
          // tempTimezone = 80;
        }
        if (count == WIT)
        {
          ptr = PSTR("WIT (+9)");
          // tempTimezone = 90;
        }

        strcpy_P(str, ptr);

        matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);
        x1 = (matrix.width() - (w - 4)) / 2;
        y1 = (matrix.height() / 2 - h) / 2 + h - 1;

        if (state500ms)
        {
          // matrix.fillRect(x1, y1, w - 5, h, 0);
          ptr = PSTR("");
        }
      }
      else
      {
        if (_configLocation.timezone == 70)
        {
          ptr = PSTR("WIB (+7)");
          count = 0;
        }
        else if (_configLocation.timezone == 80)
        {
          ptr = PSTR("WITA (+8)");
          count = 1;
        }
        else if (_configLocation.timezone == 90)
        {
          ptr = PSTR("WIT (+9)");
          count = 2;
        }
        else
        {
          ptr = PSTR("Unknown");
        }
      }

      strcpy_P(str, ptr);

      matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);
      x1 = (matrix.width() - (w - 4)) / 2;
      y1 = (matrix.height() / 2 - h) / 2 + h - 1;
      //update x1 and y1 value
      // matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

      matrix.setCursor(x1, y1 + 8);
      matrix.print(str);
    }

    // -------------------------------------------------------------------
    // MODE 2, PAGE 1
    // SETTING LOCATION
    // -------------------------------------------------------------------
    else if (currentPageMode2 == 1)
    {
      //clear Screen
      matrix.clearDisplay();

      matrix.setFont(&bold_led_board_7_regular4pt7b);
      matrix.setFont(&FiveBySeven5pt7b);
      matrix.setFont(&F14LED7pt8b);
      matrix.setFont(&RepetitionScrolling5pt8b);
      matrix.setFont(&TomThumb);

      // uint16_t x0 = 1;
      // uint16_t y0 = 6;
    }

    // -------------------------------------------------------------------
    // MODE 2, PAGE 2
    // SETTING DATE
    // -------------------------------------------------------------------
    if (currentPageMode2 == 2)
    {
      //clear Screen
      matrix.clearDisplay();

      static bool yesNo = false;

      static uint16_t yr = year(localTime);
      static int8_t mo = month(localTime);
      static int8_t d = day(localTime);

      static int16_t xYr = 0;
      static int16_t xMo = 0;
      static int16_t xD = 0;
      static int16_t xYesNo = 0;

      if (currentPageMode2_old != currentPageMode2)
      {
        currentPageMode2_old = currentPageMode2;
        digitalClockDisplay();
        Serial.print(F("> EDIT MODE, PAGE "));
        Serial.println(currentPageMode2);

        // store current page
        _ledMatrixSettings.pagemode2 = currentPageMode2;

        // reset static variables
        stateEdit = false;
        stateSwitch_old = false;
        yesNo = false;
        pos = -1;
        yr = year(localTime);
        mo = month(localTime);
        d = day(localTime);
        // hr = hour(localTime);
        // min = minute(localTime);
        // sec = second(localTime);

        xYr = 0;
        xMo = 0;
        xD = 0;
        xYesNo = 0;
      }

      static bool stateSwitch_old = false;
      if (stateSwitch != stateSwitch_old)
      {
        stateSwitch_old = stateSwitch;

        if (stateSwitch == true)
        {
          startMillis = millis();
        }
        else if (stateSwitch == false)
        {
          if (enterEditModeFromShortcut)
          {
            enterEditModeFromShortcut = false;
          }
          else if (millis() <= startMillis + 500)
          {
            stateEdit = true;
            pos++;
            if (pos > 3)
            {
              // reset
              pos = -1;
              stateEdit = false;

              if (yesNo)
              {
                // calculate utcTime
                uint16_t _yr = yr;
                uint8_t _mo = mo;
                uint8_t _d = d;
                uint8_t _hr = hour(localTime);
                uint8_t _min = minute(localTime);
                uint8_t _sec = second(localTime);
                RtcDateTime dt(_yr, _mo, _d, _hr, _min, _sec);

                time_t utcTimestamp = dt.Epoch32Time() - ((_configLocation.timezone / 10.0) * 3600);
                PRINT("%lu\r\n", utcTimestamp);

                // add 1 second correction
                // to compensate delay etc.
                utcTimestamp = utcTimestamp + 1;

                // RtcDateTime timeToSetToRTC;
                dt.InitWithEpoch32Time(utcTimestamp);

                // sync to RTC
                Rtc.SetDateTime(dt);
                lastSyncRTC = utcTimestamp;

                // sync system time (UTC)
                setTime(utcTimestamp);
                _lastSyncd = utcTimestamp;

                // reset to no
                yesNo = false;

                tone1 = HIGH;
              }
            }
          }
        }
      }

      if (stateSwitch == false && millis() <= startMillis + 500)
      {
      }

      if (stateEdit)
      {
        if (pos == 0)
        {
          if (encoder0PosIncreasing)
          {
            yr++;
            if (yr > 2100)
            {
              yr = 1970;
            }
          }
          else if (encoder0PosDecreasing)
          {
            yr--;
            if (yr < 1970)
            {
              yr = 2100;
            }
          }
        }
        else if (pos == 1)
        {
          if (encoder0PosIncreasing)
          {
            mo++;
            if (mo > 12)
            {
              mo = 1;
            }
          }
          else if (encoder0PosDecreasing)
          {
            mo--;
            if (mo < 1)
            {
              mo = 12;
            }
          }
        }
        else if (pos == 2)
        {
          // 1. January - 31 days
          // 2. February - 28 days in a common year and 29 days in leap years
          // 3. March - 31 days
          // 4. April - 30 days
          // 5. May - 31 days
          // 6. June - 30 days
          // 7. July - 31 days
          // 8. August - 31 days
          // 9. September - 30 days
          // 10. October - 31 days
          // 11. November - 30 days
          // 12. December - 31 days

          int8_t maxDay = 31;

          if (mo == 1 || mo == 3 || mo == 5 || mo == 7 || mo == 8 || mo == 10 || mo == 12)
          {
            maxDay = 31;
          }
          else if (mo == 4 || mo == 6 || mo == 9 || mo == 11)
          {
            maxDay = 30;
          }
          else if (mo == 2)
          {
            if (yr % 4 == 0)
            {
              maxDay = 29;
            }
            else
            {
              maxDay = 28;
            }
          }

          if (encoder0PosIncreasing)
          {
            d++;
            if (d > maxDay)
            {
              d = 1;
            }
          }
          else if (encoder0PosDecreasing)
          {
            d--;
            if (d < 1)
            {
              d = maxDay;
            }
          }
        }
        else if (pos == 3)
        {
          if (encoder0PosIncreasing || encoder0PosDecreasing)
          {
            yesNo = !yesNo;
          }
        }
      }

      char str[16];
      const GFXfont *font;
      int16_t x, y;
      uint16_t w, h;

      // -------
      // Row 0
      // -------
      bool ROW_0 = true;

      if (ROW_0)
      {
        if (stateEdit == false)
        {
          const char *ptr = PSTR("TANGGAL");
          strcpy_P(str, ptr);

          font = &TomThumb;
          matrix.setFont(font);

          matrix.getTextBounds(str, 0, 0, &x, &y, &w, &h);

          // set starting row row based on font type
          if (font == &TomThumb)
          {
            x = (matrix.width() - w) / 2 + 4;
            y = (matrix.height() / 2 - h) / 2 + h;
          }
          else
          {
            x = (matrix.width() - w) / 2;
            y = (matrix.height() / 2 - h) / 2 + h;
          }

          //Print to led matrix
          matrix.setCursor(x, y);
          matrix.print(str);
        }
        else if (stateEdit)
        {
          const char *ptr = PSTR("Simpan: ");
          strcpy_P(str, ptr);

          font = &Org_01;
          matrix.setFont(font);

          x = 1;
          y = 5;

          // Print to led matrix
          matrix.setCursor(x, y);
          matrix.print(str);

          // Print yesNo
          xYesNo = matrix.getCursorX();

          if (yesNo)
          {
            ptr = PSTR("Yes");
          }
          else
          {
            ptr = PSTR("No");
          }
          strcpy_P(str, ptr);
          matrix.print(str);
        }
      }

      // -------
      // Row 1
      // -------
      bool ROW_1 = true;

      if (ROW_1)
      {
        // matrix.setFont(&TomThumb);
        // matrix.setFont(&bold_led_board_7_regular4pt7b);
        // matrix.setFont(&RepetitionScrolling5pt8b);
        matrix.setFont(&FiveBySeven5pt7b);
        // matrix.setFont(&Org_01);

        int16_t x = 0;
        int16_t y = 14;

        if (stateEdit == false)
        {
          //Print Year
          yr = year(localTime);
          sprintf(str, "%02d", yr);
          xYr = x;
          matrix.setCursor(xYr, y);
          matrix.print(str);

          // //Print separator
          // x0 = x0 + 22;
          // matrix.setCursor(x0, y);
          // matrix.print(" ");

          //Print month
          mo = month(localTime);
          sprintf(str, "%s", monthShortStr(mo));
          x = x + 27;
          xMo = x;
          matrix.setCursor(xMo, y);
          matrix.print(str);

          // //Print separator
          // x0 = x0 + 11;
          // matrix.setCursor(x0, y);
          // matrix.print(FPSTR(" "));

          //Print day
          d = day(localTime);
          sprintf(str, "%02d", d);
          x = x + 21;
          xD = x;
          matrix.setCursor(xD, y);
          matrix.print(str);
        }
        else if (stateEdit)
        {
          //Print hour
          sprintf(str, "%02d", yr);
          matrix.setCursor(xYr, y);
          matrix.print(str);

          // //Print separator
          // matrix.setCursor(xYr + 11, y);
          // matrix.print("-");

          //Print min
          sprintf(str, "%s", monthShortStr(mo));
          matrix.setCursor(xMo, y);
          matrix.print(str);

          // //Print separator
          // matrix.setCursor(xMo + 11, y);
          // matrix.print(FPSTR("-"));

          //Print sec
          sprintf(str, "%02d", d);
          matrix.setCursor(xD, y);
          matrix.print(str);
        }
      }

      // blink
      if (state250ms && stateEdit)
      {
        int16_t x = 0;
        int16_t y = 0;
        uint16_t w = 0;
        uint16_t h = 0;

        if (pos == 0) // year
        {
          x = xYr;
          y = 8;
          w = 24;
          h = 7;
        }
        else if (pos == 1) // month
        {
          x = xMo;
          y = 8;
          w = 18;
          h = 7;
        }
        else if (pos == 2) // day
        {
          x = xD;
          y = 8;
          w = 12;
          h = 7;
        }
        if (pos == 3)
        {
          x = xYesNo;
          y = 1;
          w = 17;
          h = 5;
        }
        matrix.fillRect(x, y, w, h, false);
        // matrix.drawRect(x, y-1, w+1, h+2, 1);
      }
    }

    // -------------------------------------------------------------------
    // MODE 2, PAGE 3
    // SETTING TIME
    // -------------------------------------------------------------------
    if (currentPageMode2 == 3)
    {
      //clear Screen
      matrix.clearDisplay();

      static bool yesNo = false;
      static int8_t hr = hour(localTime);
      static int8_t min = minute(localTime);
      static int8_t sec = second(localTime);
      static int16_t xHr = 0;
      static int16_t xMin = 0;
      static int16_t xSec = 0;
      static int16_t xYesNo = 0;

      if (currentPageMode2_old != currentPageMode2)
      {
        currentPageMode2_old = currentPageMode2;
        digitalClockDisplay();
        Serial.print(F("> EDIT MODE, PAGE "));
        Serial.println(currentPageMode2);

        // store current page
        _ledMatrixSettings.pagemode2 = currentPageMode2;

        // reset static variables
        stateEdit = false;
        stateSwitch_old = false;
        yesNo = false;
        pos = -1;
        hr = hour(localTime);
        min = minute(localTime);
        sec = second(localTime);
        // d = day(localTime);
        // mo = month(localTime);
        // yr = year(localTime);

        xHr = 0;
        xMin = 0;
        xSec = 0;
        xYesNo = 0;
      }

      static bool stateSwitch_old = false;
      if (stateSwitch != stateSwitch_old)
      {
        stateSwitch_old = stateSwitch;
        if (stateSwitch == true)
        {
          startMillis = millis();
        }
        else if (stateSwitch == false)
        {
          if (enterEditModeFromShortcut)
          {
            enterEditModeFromShortcut = false;
          }
          else if (millis() <= startMillis + 500)
          {
            stateEdit = true;
            pos++;
            if (pos > 3)
            {
              // reset
              pos = -1;
              stateEdit = false;

              if (yesNo)
              {
                // calculate utcTime
                uint16_t y = year(localTime);
                uint8_t mo = month(localTime);
                uint8_t d = day(localTime);
                uint8_t h = hr;
                uint8_t mi = min;
                uint8_t s = sec;
                RtcDateTime dt(y, mo, d, h, mi, s);

                time_t utcTimestamp = dt.Epoch32Time() - ((_configLocation.timezone / 10.0) * 3600);
                PRINT("%lu\r\n", utcTimestamp);

                // RtcDateTime timeToSetToRTC;
                dt.InitWithEpoch32Time(utcTimestamp);

                // sync to RTC
                Rtc.SetDateTime(dt);
                lastSyncRTC = utcTimestamp;

                // sync system time (UTC)
                setTime(utcTimestamp);
                _lastSyncd = utcTimestamp;

                tone1 = HIGH;
              }
              else if (yesNo == false)
              {
                // reset to yes
                yesNo = false;
              }
            }
          }
        }
      }

      if (stateEdit)
      {
        if (pos == 0)
        {
          if (encoder0PosIncreasing)
          {
            hr++;
            if (hr > 23)
            {
              hr = 0;
            }
          }
          else if (encoder0PosDecreasing)
          {
            hr--;
            if (hr < 0)
            {
              hr = 23;
            }
          }
        }
        else if (pos == 1)
        {
          if (encoder0PosIncreasing)
          {
            min++;
            if (min > 59)
            {
              min = 0;
            }
          }
          else if (encoder0PosDecreasing)
          {
            min--;
            if (min < 0)
            {
              min = 59;
            }
          }
        }
        else if (pos == 2)
        {
          if (encoder0PosIncreasing)
          {
            sec++;
            if (sec > 59)
            {
              sec = 0;
            }
          }
          else if (encoder0PosDecreasing)
          {
            sec--;
            if (sec < 0)
            {
              sec = 59;
            }
          }
        }
        else if (pos == 3)
        {
          if (encoder0PosIncreasing || encoder0PosDecreasing)
          {
            yesNo = !yesNo;
          }
        }
      }

      // -------
      // Row 0
      // -------

      char str[16];

      if (stateEdit == false)
      {
        const char *ptr = PSTR("TIME");
        strcpy_P(str, ptr);

        int16_t x0, y0;
        uint16_t w, h;
        matrix.setFont(&TomThumb);
        matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);
        uint16_t x1 = (matrix.width() - (w - 4)) / 2;
        uint16_t y1 = (matrix.height() / 2 - h) / 2 + h - 0;
        //update x1 and y1 value
        matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

        //Print to led matrix
        matrix.setCursor(1, y1);
        matrix.print(str);
      }
      else if (stateEdit)
      {
        const char *ptr = PSTR("SAVE: ");
        strcpy_P(str, ptr);

        matrix.setFont(&TomThumb);

        // Print to led matrix
        matrix.setCursor(1, 6);
        matrix.print(str);

        // Print yesNo
        xYesNo = matrix.getCursorX();

        if (yesNo)
        {
          ptr = PSTR("YES");
          strcpy_P(str, ptr);
          matrix.print(str);
        }
        else
        {
          ptr = PSTR("NO");
          strcpy_P(str, ptr);
          matrix.print(str);
        }
      }

      // -------
      // Row 1
      // -------

      // matrix.setFont(&TomThumb);
      // matrix.setFont(&bold_led_board_7_regular4pt7b);
      matrix.setFont(&RepetitionScrolling5pt8b);
      // matrix.setFont(&FiveBySeven5pt7b);
      // matrix.setFont(&Org_01);

      if (stateEdit == false)
      {
        int16_t x0 = 1;
        int16_t y0 = 6;

        // //Print day
        // sprintf(str, "%02d", d);
        // matrix.setCursor(x0, y0);
        // matrix.print(str);

        // //Print month
        // x0 = x0 + 9;
        // sprintf(str, "%s", monthShortStr(mo));
        // matrix.setCursor(x0, y0);
        // matrix.print(str);

        // //Print year
        // x0 = x0 + 13;
        // sprintf(str, "%04d", yr);
        // matrix.setCursor(x0, y0);
        // matrix.print(str);

        //Print hour
        hr = hour(localTime);
        sprintf(str, "%02d", hr);
        x0 = 1;
        matrix.setCursor(x0, y0 + 8);
        matrix.print(str);
        xHr = x0;

        //Print ":"
        x0 = x0 + 11;
        matrix.setCursor(x0, y0 + 8);
        matrix.print(":");

        //Print min
        min = minute(localTime);
        sprintf(str, "%02d", min);
        x0 = x0 + 6;
        matrix.setCursor(x0, y0 + 8);
        matrix.print(str);
        xMin = x0;

        //Print ":"
        x0 = x0 + 11;
        matrix.setCursor(x0, y0 + 8);
        matrix.print(FPSTR(":"));

        //Print sec
        sec = 0;
        sprintf(str, "%02d", second(localTime));
        x0 = x0 + 6;
        matrix.setCursor(x0, y0 + 8);
        matrix.print(str);
        xSec = x0;
      }
      else if (stateEdit)
      {
        int16_t y = 14;

        //Print hour
        sprintf(str, "%02d", hr);
        matrix.setCursor(xHr, y);
        matrix.print(str);

        //Print ":"
        matrix.setCursor(xHr + 11, y);
        matrix.print(":");

        //Print min
        sprintf(str, "%02d", min);
        matrix.setCursor(xMin, y);
        matrix.print(str);

        //Print ":"
        matrix.setCursor(xMin + 11, y);
        matrix.print(FPSTR(":"));

        //Print sec
        sprintf(str, "%02d", sec);
        matrix.setCursor(xSec, y);
        matrix.print(str);
      }

      // blink
      if (state250ms && stateEdit)
      {
        int16_t x = 0;
        int16_t y = 8;
        uint16_t w = 11;
        uint16_t h = 7;

        if (pos == 0)
        {
          x = xHr;
        }
        else if (pos == 1)
        {
          x = xMin;
        }
        else if (pos == 2)
        {
          x = xSec;
        }
        if (pos == 3)
        {
          x = xYesNo;
          y = 1;
          w = 11;
          h = 5;
        }
        matrix.fillRect(x, y, w, h, 0);
      }
    }
  }
}

void refreshDisplay(int times)
{
  // analogWrite(2, 200);
  // static int state;
  // static unsigned long start;
  matrix.on();
  for (int i = 0; i < times * (matrix.height() + 0); i++)
  //for (int i = 0; i < times * 16; i++)
  {
    matrix.scan();
  }

  // workaround to give more time for Led on the last row to lit
  //delayMicroseconds(50);
  matrix.off();
}

void step3()
{
  static int staticX = 64;
  static int count;

  int times = 1;

  if (_ledMatrixSettings.scrollrow_0 == true || _ledMatrixSettings.scrollrow_1 == true)
  {

    if (_ledMatrixSettings.scrollspeed > 25)
    {
      //times = 1;
      count++;
      //int maxCount = 6; // scan rate 30
      //int maxCount = 16;
      int maxCount = _ledMatrixSettings.scrollspeed - 20;
      if (count < maxCount)
      {
        //process_runningled_page();
        refreshDisplay(times);
      }
      else if (count == maxCount)
      {
        staticX = staticX - 1;

        if (staticX < -wText)
        {
          staticX = 64;
        }

        X = staticX;

        count = 0;
        //refreshDisplay(1);
        process_runningled_page();
        refreshDisplay(times);
      }
    }
    else
    {
      if (_ledMatrixSettings.scrollspeed > 20 && _ledMatrixSettings.scrollspeed <= 25)
      {
        times = 2;
      }
      count = 0;
      staticX = staticX - 1;

      if (staticX < -wText)
      {
        staticX = 64;
      }

      X = staticX;
      process_runningled_page();
      refreshDisplay(times);
    }
  }
  else
  {
    count = 0;
    X = 0;
    staticX = 64;
    process_runningled_page();
    refreshDisplay(times);
  }

  //refreshDisplay(3);
}

void runningled_loop()
{
  scrollSpeed = _ledMatrixSettings.scrollspeed;
  if (scrollSpeed > 25)
  {
    scrollSpeed = 11;
  }

  unsigned long Timer = millis();
  static unsigned long lastTimer;

  if (millis() - lastTimer > scrollSpeed)
  {
    step3();
    lastTimer = Timer;
  }
}



void TMP102_loop()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  float temperature;
  // boolean alertPinState;
  // boolean alertRegisterState;

  // Turn sensor on to start temperature measurement.
  // Current consumtion typically ~10uA.
  //TMP102.wakeup();

  // read temperature data
  temperature = TMP102.readTempC();

  //round the float value to one decimal while converting to char array
  char buf[5];
  dtostrf(temperature, 1, 1, buf);

  // Check for Alert
  //alertPinState = digitalRead(ALERT_PIN); // read the Alert from pin
  // alertRegisterState = TMP102.alert(); // read the Alert from register

  // Place sensor in sleep mode to save power.
  // Current consumtion typically <0.5uA.
  //TMP102.sleep();

  if ((strcmp(buf, bufTempSensor) != 0))
  {
    dtostrf(temperature, 1, 1, bufTempSensor);
    // Print temperature and alarm state
    DEBUGLOG("%s\r\n", getDateTimeString(localTime));
    DEBUGLOG("> Room Temp: ");
    DEBUGLOG("%s\r\n", bufTempSensor);
    DEBUGLOG("\xC2\xB0"); //Ãƒâ€šÃ‚Âº Ãƒâ€šÃ‚Â°
    DEBUGLOG("C");
    //DEBUGLOG("\tAlert Pin: ");
    //DEBUGLOG(alertPinState);
    // DEBUGLOG("\tAlert Register: ");
    // DEBUGLOG("%d\r\n", alertRegisterState);
  }
}

void GET_RTC_TEMPERATURE()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  static float RtcTemp_old;

  RtcTemperature temp = Rtc.GetTemperature();
  float RtcTemp = temp.AsFloat();

  if (RtcTemp != RtcTemp_old)
  {
    RtcTemp_old = RtcTemp;

    //char buf[5];
    dtostrf(RtcTemp, 1, 1, bufRtcTemp);

    digitalClockDisplay();
    Serial.print(F("> RTC Temp: "));
    Serial.print(RtcTemp);
    Serial.print("\xC2\xB0");
    Serial.println(F("C"));
  }
}

void AlarmTrigger()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  static boolean callAlarmSholatType1 = LOW;

  if ((HOUR == 0 && MINUTE == 10) && SECOND == 0)
  {
    callAlarmSholatType1 = HIGH;
  }
  if ((HOUR == 0 && MINUTE == 5) && SECOND == 0)
  {
    callAlarmSholatType1 = HIGH;
  }

  if (callAlarmSholatType1 == HIGH)
  {
    static byte count = 0;
    if (count < 3)
    {
      alarmState = HIGH;
      count++;
    }
    else if (count >= 3)
    {
      alarmState = LOW;
      callAlarmSholatType1 = LOW;
      count = 0;
    }
  }

  //sound the Alarm when current timestamp match
  //with one of sholattime's timestamp
  static boolean callAlarmSholatType2 = LOW;
  time_t t = local_time();
  for (int i = 0; i < TimesCount; i++)
  {
    if (i != Sunset)
    {
      if (t == sholat.timestampSholatTimesToday[i])
      {
        callAlarmSholatType2 = HIGH;
      }
    }
  }

  if (callAlarmSholatType2 == HIGH)
  {
    tone10 = HIGH;
    static byte count = 0;
    if (count < 4)
    {
      duration = 500;
      count++;
    }
    else if (count == 4)
    {
      duration = 2000;
      count++;
    }
    else if (count >= 5)
    {
      callAlarmSholatType2 = LOW;
      count = 0;
    }
  }
}

void PageAutomaticMode()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  time_t adzanEndTime = currentSholatTime + (60 * _ledMatrixSettings.adzanwaittime);
  time_t iqamahTime = adzanEndTime + (60 * _ledMatrixSettings.iqamahwaittime);
  if (_ledMatrixSettings.pagemode == Automatic)
  {
    if (atof(bufWatt) >= 2400)
    {
      currentPageMode0 = 4;
    }

    else if ((nextSholatTime - localTime) <= (60 * 50))
    {
      currentPageMode0 = 8;
      //PRINT("%s nextSholatTime= %lu localTime= %lu diff= %d\n", "Case A", nextSholatTime, localTime, nextSholatTime - localTime);
    }

    else if (localTime < adzanEndTime)
    {
      currentPageMode0 = 2;
    }

    else if (localTime <= iqamahTime)
    {
      currentPageMode0 = 11;
    }

    else if (localTime <= currentSholatTime + (60 * 30))
    {
      currentPageMode0 = 2;
      //PRINT("%s currentSholatTime= %lu localTime= %lu diff= %d\n", "Case B", currentSholatTime, localTime, localTime - currentSholatTime);
    }

    else if (atof(bufWatt) >= 2000)
    {
      currentPageMode0 = 4;
    }
    else
    {
      currentPageMode0 = _ledMatrixSettings.pagemode0;
    }
  }
  else if (_ledMatrixSettings.pagemode == Manual)
  {
    MODE = _ledMatrixSettings.operatingmode;
    currentPageMode0 = _ledMatrixSettings.pagemode0;
    currentPageMode1 = _ledMatrixSettings.pagemode1;
    currentPageMode2 = _ledMatrixSettings.pagemode2;
  }
}

void RtcStatusLoop()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  static byte rtcStatus_old = RtcStatus();
  byte rtcStatus = RtcStatus();
  if (rtcStatus != rtcStatus_old)
  {
    rtcStatus_old = rtcStatus;

    digitalClockDisplay();
    Serial.print(F("> "));
    Serial.println(GetRtcStatusString());

    if (rtcStatus == RTC_TIME_VALID)
    {
      RTC_OK = true;
    }
    else
    {
      RTC_OK = false;
    }
  }
}

void ProcessSholatEverySecond()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  static time_t t_nextMidnight = 0;

  time_t t = nextMidnight(local_time());

  if (t != t_nextMidnight)
  {
    t_nextMidnight = t;

    process_sholat();
  }

  process_sholat_2nd_stage();

  static int NEXTTIMEID_old = 100;

  if (NEXTTIMEID != NEXTTIMEID_old)
  {
    NEXTTIMEID_old = NEXTTIMEID;
  }
}

void CheckNewTimeSettingsReceived()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  static uint8_t timezone_old = _configLocation.timezone;
  static bool dst_old = _configTime.dst;
  static bool enableNtp_old = _configTime.enablentp;

  static char ntpserver_0_old[48];
  static char ntpserver_1_old[48];
  static char ntpserver_2_old[48];

  // static char* ntpserver_0_old = _config.ntpserver_0;
  // static char* ntpserver_1_old = _config.ntpserver_1;
  // static char* ntpserver_2_old = _config.ntpserver_2;

  static bool enableRtc_old = _configTime.enablertc;
  static uint32_t syncinterval_old = _configTime.syncinterval;

  if (
      _configLocation.timezone != timezone_old ||
      _configTime.dst != dst_old ||
      _configTime.enablentp != enableNtp_old ||
      strncmp(_configTime.ntpserver_0, ntpserver_0_old, sizeof(_configTime.ntpserver_0)) != 0 ||
      strncmp(_configTime.ntpserver_1, ntpserver_1_old, sizeof(_configTime.ntpserver_1)) != 0 ||
      strncmp(_configTime.ntpserver_2, ntpserver_2_old, sizeof(_configTime.ntpserver_2)) != 0 ||
      _configTime.enablertc != enableRtc_old ||
      _configTime.syncinterval != syncinterval_old

  )
  {

    digitalClockDisplay();
    Serial.println(F(">"));
    digitalClockDisplay();
    Serial.print(F("> "));
    Serial.println(F("*** New TIME settings received ***"));
    digitalClockDisplay();
    Serial.println(F(">"));

    if (_configLocation.timezone != timezone_old)
    {
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.print(F("Timezone updated to: "));
      Serial.println(_configLocation.timezone);

      process_sholat();
    }

    if (_configTime.dst != dst_old)
    {
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.print(F("DST settings updated to: "));
      Serial.println(_configTime.dst);

      process_sholat();
    }

    if (_configTime.enablentp != enableNtp_old)
    {
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.print(F("NTP settings updated to: "));
      Serial.println(_configTime.enablentp);

      if (_configTime.enablentp == true)
      {
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("NTP_OK set to TRUE"));

        NTP_OK = true;
      }
      else if (_configTime.enablentp == false)
      {
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("NTP_OK set to FALSE"));

        NTP_OK = false;
      }
    }

    if (_configTime.enablertc != enableRtc_old)
    {
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.print(F("RTC settings updated to: "));
      Serial.println(_configTime.enablertc);

      if (_configTime.enablertc == true)
      {
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("RTC_OK set to TRUE"));

        RTC_OK = true;
      }
      else if (_configTime.enablertc == false)
      {
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("RTC_OK set to FALSE"));

        RTC_OK = false;
      }
    }

    if (_configTime.syncinterval != syncinterval_old)
    {
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.print(F("LONG sync interval updated to: "));
      Serial.println(_configTime.syncinterval);

      setInterval(_configTime.syncinterval);
    }

    // update SDK NTP server name
    if (
        strncmp(_configTime.ntpserver_0, ntpserver_0_old, sizeof(_configTime.ntpserver_0)) != 0 ||
        strncmp(_configTime.ntpserver_1, ntpserver_1_old, sizeof(_configTime.ntpserver_1)) != 0 ||
        strncmp(_configTime.ntpserver_2, ntpserver_2_old, sizeof(_configTime.ntpserver_2)) != 0)
    {
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.print(F("SDK NTP Server-0 updated to: "));
      Serial.print(_configTime.ntpserver_0);
      Serial.println(F(", reinitialize sntp"));

      sntp_stop();
      sntp_setservername(0, _configTime.ntpserver_0);

      //char ntpserver_1[] = "1.id.pool.ntp.org";
      sntp_setservername(1, _configTime.ntpserver_1); //  set server  1 by  domain  name

      //char ntpserver_2[] = "192.168.10.1";
      sntp_setservername(2, _configTime.ntpserver_2); //  set server  2 by  domain  name

      sntp_init();

      delay(1000);
    }

    //update old values
    timezone_old = _configLocation.timezone;
    dst_old = _configTime.dst;
    enableNtp_old = _configTime.enablentp;
    strlcpy(ntpserver_0_old, _configTime.ntpserver_0, sizeof(ntpserver_0_old));
    strlcpy(ntpserver_1_old, _configTime.ntpserver_1, sizeof(ntpserver_1_old));
    strlcpy(ntpserver_2_old, _configTime.ntpserver_2, sizeof(ntpserver_2_old));
    enableRtc_old = _configTime.enablertc;
    syncinterval_old = _configTime.syncinterval;
  }
}


bool save_system_info()
{
  PRINT("%s\r\n", __PRETTY_FUNCTION__);

  // const char* pathtofile = PSTR(pgm_filesystemoverview);

  File file;
  if (!SPIFFS.exists(FPSTR(pgm_systeminfofile)))
  {
    file = SPIFFS.open(FPSTR(pgm_systeminfofile), "w");
    if (!file)
    {
      PRINT("Failed to open config file for writing\r\n");
      file.close();
      return false;
    }
    //create blank json file
    PRINT("Creating user config file for writing\r\n");
    file.print("{}");
    file.close();
  }
  //get existing json file
  file = SPIFFS.open(FPSTR(pgm_systeminfofile), "w");
  if (!file)
  {
    PRINT("Failed to open config file");
    return false;
  }

  const char *the_path = PSTR(__FILE__);
  // const char *_compiletime = PSTR(__TIME__);

  int slash_loc = pgm_lastIndexOf('/', the_path); // index of last '/'
  if (slash_loc < 0)
    slash_loc = pgm_lastIndexOf('\\', the_path); // or last '\' (windows, ugh)

  int dot_loc = pgm_lastIndexOf('.', the_path); // index of last '.'
  if (dot_loc < 0)
    dot_loc = pgm_lastIndexOf(0, the_path); // if no dot, return end of string

  int lenPath = strlen(the_path);
  int lenFileName = (lenPath - (slash_loc + 1));

  char fileName[lenFileName];
  //Serial.println(lenFileName);
  //Serial.println(sizeof(fileName));

  int j = 0;
  for (int i = slash_loc + 1; i < lenPath; i++)
  {
    uint8_t b = pgm_read_byte(&the_path[i]);
    if (b != 0)
    {
      fileName[j] = (char)b;
      //Serial.print(fileName[j]);
      j++;
      if (j >= lenFileName)
      {
        break;
      }
    }
    else
    {
      break;
    }
  }
  //Serial.println();
  //Serial.println(j);
  fileName[lenFileName] = '\0';

  //const char* _compiledate = PSTR(__DATE__);
  int lenCompileDate = strlen_P(PSTR(__DATE__));
  char compileDate[lenCompileDate];
  strcpy_P(compileDate, PSTR(__DATE__));

  int lenCompileTime = strlen_P(PSTR(__TIME__));
  char compileTime[lenCompileTime];
  strcpy_P(compileTime, PSTR(__TIME__));

  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  SPIFFS.info(fs_info);

  root[FPSTR(pgm_totalbytes)] = fs_info.totalBytes;
  root[FPSTR(pgm_usedbytes)] = fs_info.usedBytes;
  root[FPSTR(pgm_blocksize)] = fs_info.blockSize;
  root[FPSTR(pgm_pagesize)] = fs_info.pageSize;
  root[FPSTR(pgm_maxopenfiles)] = fs_info.maxOpenFiles;
  root[FPSTR(pgm_maxpathlength)] = fs_info.maxPathLength;

  root[FPSTR(pgm_filename)] = fileName;
  root[FPSTR(pgm_compiledate)] = compileDate;
  root[FPSTR(pgm_compiletime)] = compileTime;
  root[FPSTR(pgm_lastboot)] = getLastBootStr();
  root[FPSTR(pgm_chipid)] = ESP.getChipId();
  root[FPSTR(pgm_cpufreq)] = ESP.getCpuFreqMHz();
  root[FPSTR(pgm_sketchsize)] = ESP.getSketchSize();
  root[FPSTR(pgm_freesketchspace)] = ESP.getFreeSketchSpace();
  root[FPSTR(pgm_flashchipid)] = ESP.getFlashChipId();
  root[FPSTR(pgm_flashchipmode)] = ESP.getFlashChipMode();
  root[FPSTR(pgm_flashchipsize)] = ESP.getFlashChipSize();
  root[FPSTR(pgm_flashchiprealsize)] = ESP.getFlashChipRealSize();
  root[FPSTR(pgm_flashchipspeed)] = ESP.getFlashChipSpeed();
  root[FPSTR(pgm_cyclecount)] = ESP.getCycleCount();
  root[FPSTR(pgm_corever)] = ESP.getCoreVersion();
  root[FPSTR(pgm_sdkver)] = ESP.getSdkVersion();
  root[FPSTR(pgm_bootmode)] = ESP.getBootMode();
  root[FPSTR(pgm_bootversion)] = ESP.getBootVersion();
  root[FPSTR(pgm_resetreason)] = ESP.getResetReason();

  root.prettyPrintTo(file);
  file.flush();
  file.close();
  return true;
}


void setup()
{
#ifdef SET_ANALOG_FREQUENCY
  uint16_t cpuFreq = ESP.getCpuFreqMHz();
  uint8_t cpuFreqFactor;
  if (cpuFreq == 80)
  {
    cpuFreqFactor = 2;
  }
  else if (cpuFreq == 160)
  {
    cpuFreqFactor = 1;
  }

  ANALOGWRITE_OPERATING_FREQUENCY = cpuFreqFactor * ANALOGWRITE_BASE_FREQUENCY;
  analogWriteFreq(ANALOGWRITE_OPERATING_FREQUENCY);
#endif

  Serial.begin(115200);
  Serial.println();
  Serial.setDebugOutput(0);

  display_srcfile_details();

  Serial.print(F("Reset reason: "));
  Serial.println(ESP.getResetReason());
  Serial.println();
  Serial.print(F("Reset info: "));
  Serial.println(ESP.getResetInfo());
  Serial.print(F("Core version: "));
  Serial.println(ESP.getCoreVersion());
  Serial.print(F("SDK version: "));
  Serial.println(ESP.getSdkVersion());
  Serial.print(F("CPU Freq: "));
  Serial.println(ESP.getCpuFreqMHz());
  Serial.println();

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  pinMode(LED_1, OUTPUT);
  digitalWrite(LED_1, LOW);

  // -------------------------------------------------------------------
  // Mount File system
  // -------------------------------------------------------------------

  Serial.println(F("Mounting FS..."));

  if (!SPIFFS.begin())
  {
    Serial.println(F("Failed to mount file system"));
    return;
  }

  // -------------------------------------------------------------------
  // Setup MQTT
  // -------------------------------------------------------------------
  Serial.println(F("//------- SETUP MQTT -------//"));

  mqtt_setup();

  //  if (!mqtt_reconnect()) {
  //    Serial.println(F("Cannot connect to MQTT!"));
  //  }

  // -------------------------------------------------------------------
  // Load configuration from EEPROM & Setup Async Server
  // -------------------------------------------------------------------
  Serial.println(F("Loading settings from EEPROM... "));
  //config_load_settings();

  //WiFi.setSleepMode(WIFI_NONE_SLEEP);

  AsyncWSBegin();
  //config_load_settings();
  Serial.println(F("OK"));

  // -------------------------------------------------------------------
  // Setup I2C stuffs
  // -------------------------------------------------------------------
  Serial.println(F("Executing I2C_ClearBus()")); //http://www.forward.com.au/pfod/ArduinoProgramming/I2C_ClearBus/index.html
  int rtn = I2C_ClearBus();                      // clear the I2C bus first before calling Wire.begin()
  if (rtn != 0)
  {
    Serial.println(F("I2C bus error. Could not clear"));
    if (rtn == 1)
    {
      Serial.println(F("SCL clock line held low"));
    }
    else if (rtn == 2)
    {
      Serial.println(F("SCL clock line held low by slave clock stretch"));
    }
    else if (rtn == 3)
    {
      Serial.println(F("SDA data line held low"));
    }
  }
  else
  { // bus clear
    // re-enable Wire
    // now can start Wire Arduino master
    Serial.println(F("bus clear, re-enable Wire.begin();"));

    //Wire.begin();
    Wire.begin(SDA, SCL);

    //Set clock to 450kHz
    //Wire.setClock(450000L);
  }

  // -------------------------------------------------------------------
  // Setup PCF8574
  // -------------------------------------------------------------------
#ifdef USE_IO_EXPANDER
  // https://github.com/RalphBacon/PCF8574-Pin-Extender-I2C/blob/master/PCF8574_InputOutputInterrupt.ino
  // PCF8574N is 'reverse' logic inasmuch it SINKS current
  // so HIGH is OFF and LOW is ON (will we remember this?)

  //IoExpanderWire.begin(SDA, SCL);
  //Specsheets say PCF8574 is officially rated only for 100KHz I2C-bus
  //IoExpanderWire.setClock(100000L);

  // Turn OFF all pins by sending a high byte (1 bit per byte)
  //  Wire.beginTransmission(IO_EXPANDER_ADDRESS);
  //  Wire.write(0xF);
  //  Wire.endTransmission();

  //Setup interrupt
  pinMode(MCU_INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(MCU_INTERRUPT_PIN), PCFInterrupt, FALLING);
#endif
  // -------------------------------------------------------------------
  // Setup LED MATRIX
  // -------------------------------------------------------------------
  matrix.begin();
  matrix.off();
  //  //matrix.setRotation(2);
  //  matrix.clearDisplay();
  //  //matrix.setFont(&TomThumb);
  //  matrix.setFont(&FiveBySeven5pt7b);
  //  matrix.setFont(&Org_01);
  //  //matrix.setTextColor(BLACK);
  matrix.setTextWrap(0);

  //tickerSetHigh.attach_ms(1, refresh);
  //tickerSetHigh.attach_ms(20, step3);

  // -------------------------------------------------------------------
  // Setup RTC
  // -------------------------------------------------------------------
  Serial.println(F("//------- RTC SETUP -------//"));
  // Wire.begin(SDA, SCL);
  // Wire.begin(); // Commented, already started by I2C_ClearBus() above

  Serial.println(F("Rtc.Begin()"));
  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Serial.print(F("compiled: "));
  printDateTime(compiled);
  Serial.println();

  Serial.println(F("Checking RTC Status"));

  if (!Rtc.IsDateTimeValid())
  {
    Serial.println(F("RTC lost confidence in the DateTime!"));

    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing

    RtcDateTime dt = Rtc.GetDateTime();
    Serial.print(F("UTC time now from RTC: "));
    printDateTime(dt);
    Serial.println();

    RtcDateTime timeRtc = dt.Epoch32Time();

    Serial.print(F("RTC timestamp (UTC): "));
    Serial.print(timeRtc);
    Serial.print(F(", "));
    Serial.println(getDateTimeString(timeRtc));

    unsigned long currMillis = millis();
    while (!Rtc.IsDateTimeValid() && millis() - currMillis <= 5000)
    {
      delay(500);
      Serial.print(">");
    }
    Serial.println();

    if (timeRtc < 1514764800)
    {
      Serial.println(F("RTC is older than 1 January 2018 :-("));

      Rtc.SetDateTime(compiled);
      Serial.println(F("RTC time has been set to compile time!"));
    }
    else if (timeRtc > 1514764800)
    {
      Serial.println(F("but strange.., RTC is seems valid, 1.e. newer than 1 January 2018..."));
      //  RtcDateTime timeToSetToRTC;
      //  timeToSetToRTC.InitWithEpoch32Time(timeRtc);
      //  Rtc.SetDateTime(timeToSetToRTC);
    }
    else if (timeRtc == compiled)
    {
      Serial.println(F("RTC is the same as compile time! (not expected but all is seems fine)"));
    }
  }
  else
  {
    Serial.println(F("RTC time is VALID"));

    RtcDateTime dt = Rtc.GetDateTime();

    time_t timeRtc = dt.Epoch32Time();

    Serial.print(F("RTC timestamp (UTC): "));
    Serial.print(timeRtc);
    Serial.print(F(", "));
    Serial.println(getDateTimeString(timeRtc));

    Serial.print(F("In human readable format: "));
    printDateTime(dt);
    Serial.println();
  }

  if (!Rtc.GetIsRunning())
  {
    Serial.println(F("RTC was not actively running, starting now"));
    Rtc.SetIsRunning(true);
  }

  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.Enable32kHzPin(true);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeClock);
  Rtc.SetSquareWavePinClockFrequency(DS3231SquareWaveClock_1Hz);

  // -------------------------------------------------------------------
  // Setup TMP102 Temperature Sensor
  // -------------------------------------------------------------------

  Serial.println(F("Setup TMP102 Temperature Sensor"));
  TMP102.begin(); // Join I2C bus

  // Initialize TMP102 settings
  // These settings are saved in the sensor, even if it loses power

  // set the number of consecutive faults before triggering alarm.
  // 0-3: 0:1 fault, 1:2 faults, 2:4 faults, 3:6 faults.
  TMP102.setFault(0); // Trigger alarm immediately

  // set the polarity of the Alarm. (0:Active LOW, 1:Active HIGH).
  TMP102.setAlertPolarity(1); // Active HIGH

  // set the sensor in Comparator Mode (0) or Interrupt Mode (1).
  TMP102.setAlertMode(0); // Comparator Mode.

  // set the Conversion Rate (how quickly the sensor gets a new reading)
  //0-3: 0:0.25Hz, 1:1Hz, 2:4Hz, 3:8Hz
  TMP102.setConversionRate(1);

  //set Extended Mode.
  //0:12-bit Temperature(-55C to +128C) 1:13-bit Temperature(-55C to +150C)
  TMP102.setExtendedMode(0);

  //set T_HIGH, the upper limit to trigger the alert on
  TMP102.setHighTempC(29.4); // set T_HIGH in C

  //set T_LOW, the lower limit to shut turn off the alert
  TMP102.setLowTempC(26.67); // set T_LOW in C

  // -------------------------------------------------------------------
  // Setup NTP
  // -------------------------------------------------------------------

  PrintTimeStatus();

  // setup sync Interval
  setInterval(15, _configTime.syncinterval);
  Serial.print(F("Set Short Sync Interval to "));
  Serial.print(getShortInterval());
  Serial.println(F(" seconds"));
  Serial.print(F("Set Long Sync Interval to "));
  Serial.print(getInterval());
  Serial.println(F(" seconds"));

  // setup sntp SDK

  /*
  sntp_stop(); // Important! Must call stop before change timezone SDK

  while (!sntp_set_timezone(0))
  {
    delay(1000);
    Serial.print(F("."));
  }
  Serial.println();

  sntp_setservername(0, _config.ntpserver_0);
  sntp_setservername(1, _config.ntpserver_1);
  sntp_setservername(2, _config.ntpserver_2);
  //  sntp_setservername(1, "0.asia.pool.ntp.org");
  //  sntp_setservername(2, "192.168.10.1");

  sntp_init();

  */

  configTime(0, 0, _configTime.ntpserver_0, _configTime.ntpserver_1);

  // this delay is seems required after first init
  // otherwise sntp SDK always fail (based on experience..)
  delay(1000);

  digitalClockDisplay(localTime);
  Serial.print(F("> "));
  Serial.print(F("NTP server-0: "));
  Serial.println(sntp_getservername(0));

  digitalClockDisplay(localTime);
  Serial.print(F("> "));
  Serial.print(F("NTP server-1: "));
  Serial.println(sntp_getservername(1));

  digitalClockDisplay(localTime);
  Serial.print(F("> "));
  Serial.print(F("NTP server-2: "));
  Serial.println(sntp_getservername(2));

  digitalClockDisplay(localTime);
  Serial.print(F("> "));
  Serial.print(F("Timezone: "));
  Serial.println(sntp_get_timezone());

  // If sntp SDK enabled, check if it is working or not

  if (_configTime.enablentp == false)
  {
    NTP_OK = false;

    digitalClockDisplay();
    Serial.println(F(">"));
    digitalClockDisplay();
    Serial.print(F("> "));
    Serial.println(F("NTP NOT ENABLED (HENCE, NTP NOT OK)"));
  }
  else if (_configTime.enablentp == true)
  {

    setSyncProvider(getNtpTimeSDK);

    if (syncNtpEventTriggered)
    {

      PrintTimeStatus();

      if (timeStatus() == timeSet)
      {

        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("NTP has succesfully set the time"));

        // store first sync and last sync
        if (_firstSync == 0)
        {
          digitalClockDisplay();
          Serial.print(F("> "));
          Serial.println();

          _firstSync = utc_time();
          digitalClockDisplay();
          Serial.print(F("> "));
          Serial.print(F("_firstSync: "));
          Serial.println(_firstSync);

          _millisFirstSync = millis();
          digitalClockDisplay();
          Serial.print(F("> "));
          Serial.print(F("millisFirstSync: "));
          Serial.println(_millisFirstSync);

          _secondsFirstSync = _millisFirstSync / 1000;
          digitalClockDisplay();
          Serial.print(F("> "));
          Serial.print(F("_secondsFirstSync: "));
          Serial.println(_secondsFirstSync);

          _lastBoot = _firstSync - _secondsFirstSync;
          digitalClockDisplay();
          Serial.print(F("> "));
          Serial.print(F("_lastBoot: "));
          Serial.println(_lastBoot);
        }

        NTP_OK = true;

        digitalClockDisplay();
        Serial.println(F(">"));
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("NTP OK"));
      }
      else
      {
        NTP_OK = false;

        digitalClockDisplay();
        Serial.println(F(">"));
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("NTP NOT OK"));
      }

      syncNtpEventTriggered = false;
    }
  }

  // Initial time sync using RTC
  if (_configTime.enablertc == false)
  {
    RTC_OK = false;

    digitalClockDisplay();
    Serial.println(F(">"));
    digitalClockDisplay();
    Serial.print(F("> "));
    Serial.println(F("RTC NOT ENABLED (HENCE, RTC NOT OK)"));
  }
  else if (_configTime.enablertc == true)
  {

    time_t t_rtc = get_time_from_rtc();

    if (syncRtcEventTriggered)
    {

      //PrintTimeStatus();

      if (t_rtc)
      {

        RTC_OK = true;

        digitalClockDisplay();
        Serial.println(F(">"));
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("RTC OK"));
      }
      else
      {
        RTC_OK = false;

        digitalClockDisplay();
        Serial.println(F(">"));
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("RTC NOT OK"));
      }

      syncRtcEventTriggered = false;
    }
  }

  if (RTC_OK)
  {
    setSyncProvider(get_time_from_rtc);
    _timeSource = TIMESOURCE_RTC;
    digitalClockDisplay();
    Serial.print(F("> "));
    Serial.println(F("Selected time source is RTC"));

    if (syncRtcEventTriggered)
    {

      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.println();

      _firstSync = utc_time();
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.print(F("_firstSync: "));
      Serial.println(_firstSync);

      _millisFirstSync = millis();
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.print(F("millisFirstSync: "));
      Serial.println(_millisFirstSync);

      _secondsFirstSync = _millisFirstSync / 1000;
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.print(F("_secondsFirstSync: "));
      Serial.println(_secondsFirstSync);

      _lastBoot = _firstSync - _secondsFirstSync;
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.print(F("_lastBoot: "));
      Serial.println(_lastBoot);

      syncRtcEventTriggered = false;
    }
  }
  else if (RTC_OK == false && NTP_OK == true)
  {
    _timeSource = TIMESOURCE_NTP;
    digitalClockDisplay();
    Serial.print(F("> "));
    Serial.println(F("Selected time source is NTP"));
  }
  //else if (RTC_OK == false && NTP_OK == false) {
  else
  {
    _timeSource = TIMESOURCE_NOT_AVAILABLE;
    Serial.print(F("*** NO VALID TIME SOURCE IS AVAILABLE ***"));
    Serial.println(F("Please check RTC chip or network / internet connection!"));
  }

  PrintTimeStatus();

  if (timeStatus() == timeSet)
  {
    setSyncInterval(getInterval());

    digitalClockDisplay();
    Serial.print(F("> "));
    Serial.print(F("Sync interval set to Long: "));
    Serial.print(getInterval());
    Serial.println(F(" seconds"));
  }
  else
  {
    setSyncInterval(getShortInterval());

    digitalClockDisplay();
    Serial.print(F("> "));
    Serial.print(F("Sync interval set to Short: "));
    Serial.print(getShortInterval());
    Serial.println(F(" seconds"));
  }

  if (RTC_OK || NTP_OK)
  {

    // save current day
    weekday_old = weekday(local_time());
    digitalClockDisplay();
    Serial.print(F("> "));
    Serial.print(F("Today is: "));
    Serial.println(weekday(local_time()));

    process_sholat();
    process_sholat_2nd_stage();
  }

  //  // -------------------------------------------------------------------
  //  // Setup MQTT
  //  // -------------------------------------------------------------------
  //  Serial.println(F("//------- SETUP MQTT -------//"));
  //
  //  mqtt_setup();
  //
  //  //  if (!mqtt_reconnect()) {
  //  //    Serial.println(F("Cannot connect to MQTT!"));
  //  //  }

  save_system_info();

  flipper250ms.attach(0.2, flip250ms);

  // -------------------------------------------------------------------
  // Setup Completed
  // -------------------------------------------------------------------

  digitalClockDisplay();
  Serial.print(F("> "));
  Serial.println(F("Setup completed!"));
}



void pingFault(void) {}

void loop()
{
  static bool enablePing = true;
  if (WiFi.status() == WL_CONNECTED && enablePing)
  {
    startPingAlive();
    enablePing = false;
  }

  utcTime = now();
  localTime = utcTime + _configLocation.timezone / 10.0 * 3600;

  static bool encoder0PosIncreasing;
  static bool encoder0PosDecreasing;
  // static int encoder0Pos = 0;
  static bool ledState;
  static unsigned long startLed;

  // i2C Interrup
  //PCFInterruptFlag = false;
  if (PCFInterruptFlag)
  {

    Wire.requestFrom(IO_EXPANDER_ADDRESS, 1);

    bool wireAvailable = Wire.available();

    byte readIOexpander = 0;
    if (wireAvailable)
    {
      readIOexpander = Wire.read();
    }
    else
    {
      PCFInterruptFlag = false;
      return;
    }

    //    // print Binary reading of IO expander
    //    static byte count;
    //    Serial.print(readIOexpander, BIN);
    //    if (readIOexpander != 0b11111111) {
    //      count++;
    //      Serial.print(" - ");
    //      Serial.println(count);
    //    } else {
    //      count = 0;
    //      Serial.print(" - ");
    //      Serial.println(count);
    //      Serial.println();
    //    }

    if (wireAvailable)
    {
      boolean rtcSQWState = bitRead(readIOexpander, RTC_SQW_PIN);
      if (rtcSQWState == 0)
      {
        //  Serial.print(rtcSQWState);
        //  Serial.print(", ");
        //  Serial.print(millis());
        //  Serial.print(", ");
        //  RtcDateTime dt = Rtc.GetDateTime();
        //  time_t timeRtc = dt.Epoch32Time();
        //  Serial.println(timeRtc);

        ledState = HIGH;
        digitalWrite(LED_1, LOW);
      }

      stateSwitch = !bitRead(readIOexpander, ENCODER_SW_PIN);

      static boolean stateA_old;
      boolean stateA = !bitRead(readIOexpander, ENCODER_CLK_PIN);
      if (stateA != stateA_old)
      {
        //Serial.print("A: ");
        //Serial.println (stateA);
        stateA_old = stateA;
        if (stateA == HIGH)
        {
          if (!bitRead(readIOexpander, ENCODER_DT_PIN) == LOW)
          {
            encoder0Pos++;
            encoder0PosIncreasing = true;
          }
          else
          {
            encoder0Pos--;
            encoder0PosDecreasing = true;
          }
          // Serial.print("Position: ");
          // Serial.println (encoder0Pos);
        }
      }
      else
      {
        encoder0PosIncreasing = false;
        encoder0PosIncreasing = false;
      }
    }

    // DO NOTE: When you write LOW to a pin on a PCF8574 it becomes an OUTPUT.
    // It wouldn't generate an interrupt if you were to connect a button to it that pulls it HIGH when you press the button.
    // Any pin you wish to use as input must be written HIGH and be pulled LOW to generate an interrupt.
    //pcf8574.write(6, LOW);
    //pcf8574.toggle(6);
    PCFInterruptFlag = false;
  }
  if (ledState == HIGH)
  {
    startLed = millis();
    ledState = LOW;
  }
  if (ledState == LOW && millis() - startLed >= 10)
  {
    digitalWrite(LED_1, HIGH);
  }

  if (wifiFirstConnected)
  {
    wifiFirstConnected = false;

    //    if (!mqttClient.connected()) {
    //      connectToMqtt();
    //    }

    //    // close MQTT if it is currently connected to broker
    //    if (!mqttClient.connected())
    //    {
    //      digitalClockDisplay();
    //      Serial.print(F("> "));
    //      Serial.println(F("Connecting to MQTT..."));
    //
    //      mqttClient.connect();
    //    }
  }

  static unsigned long prevTimer500ms;

  static time_t prevDisplay;

  if (utcTime != prevDisplay)
  {
    unsigned long currMilis = millis();
    prevTimer500ms = currMilis;
    // flipper250ms.detach();
    // flipper250ms.attach(0.25, flip250ms);
    tick1000ms = true;
    prevDisplay = utcTime;
    if (!(utcTime % 2))
    {
      // do something even
      state1000ms = true;
    }
    else
    {
      state1000ms = false;
    }
  }

  if (millis() <= prevTimer500ms + 500)
  {
    state500ms = true;
  }
  else
  {
    state500ms = false;
  }

  static boolean stateSwitch_old;
  static uint32_t timerSwitch;

  //stateSwitch = toggleSwitch.Update();

  if (stateSwitch != stateSwitch_old)
  {
    stateSwitch_old = stateSwitch;

    if (stateSwitch == HIGH)
    {
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.println(F("Switch ON"));
      timerSwitch = millis();
      // alarmState = HIGH;
      // tone0 = HIGH;
    }
    else if (stateSwitch == LOW)
    {
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.println(F("Switch OFF"));
    }
  }

  static uint16_t count;
  if (stateSwitch == HIGH)
  {
    if (millis() >= timerSwitch + 1000)
    {
      timerSwitch = millis();
      count++;
      // PRINT("count = %d\r\n", count);

      if (count >= 4)
      {
        // tone0 = HIGH;

        if (MODE == 0)
        {
          MODE = 2;
          enterEditModeFromShortcut = true;
        }
        else if (MODE == 2)
        {
          MODE = 0;
        }

        count = 0;

        tone1 = HIGH;

        // stateSwitch = LOW;
      }
    }
  }
  else if (stateSwitch == LOW)
  {
    // if (count >= 4)
    // {
    //   if (MODE == 0)
    //   {
    //     MODE = 2;
    //   }
    //   else if (MODE == 2)
    //   {
    //     MODE = 0;
    //   }
    //   tone1 = HIGH;
    // }

    // if (count == 7)
    // {
    //   MODE = 2;
    //   tone1 = HIGH;
    // }

    count = 0;
  }

  if (encoder0PosIncreasing || encoder0PosDecreasing)
  {
    digitalClockDisplay();
    Serial.print(F("> "));
    Serial.print(F("Encoder Pos: "));
    Serial.println(encoder0Pos);
    tone0 = HIGH;

    // if (encoder0PosIncreasing)
    // {
    //   if (MODE == 0)
    //   {
    //     currentPageMode0++;
    //     if (currentPageMode0 >= PageTitleMode0Count)
    //     {
    //       currentPageMode0 = 0;
    //     }
    //   }
    //   if (MODE == 1)
    //   {
    //     currentPageMode1++;
    //     if (currentPageMode1 >= PageTitleMode1Count)
    //     {
    //       currentPageMode1 = 0;
    //     }
    //   }
    //   if (MODE == 2)
    //   {
    //     currentPageMode2++;
    //     if (currentPageMode2 >= PageTitleMode2Count)
    //     {
    //       currentPageMode2 = 0;
    //     }
    //   }
    // }
    // else if (encoder0PosDecreasing)
    // {
    //   if (MODE == 0)
    //   {
    //     currentPageMode0--;
    //     if (currentPageMode0 <= 0 || currentPageMode0 >= PageTitleMode0Count)
    //     {
    //       currentPageMode0 = PageTitleMode0Count - 1;
    //     }
    //   }
    //   if (MODE == 1)
    //   {
    //     currentPageMode1--;
    //     if (currentPageMode1 <= 0 || currentPageMode1 >= PageTitleMode1Count)
    //     {
    //       currentPageMode1 = PageTitleMode1Count - 1;
    //     }
    //   }
    //   if (MODE == 2)
    //   {
    //     currentPageMode2--;
    //     if (currentPageMode2 <= 0 || currentPageMode2 >= PageTitleMode2Count)
    //     {
    //       currentPageMode2 = PageTitleMode2Count - 1;
    //     }
    //   }
    // }
  }

  //check, update and print time status if necessary
  static byte timeStatus_old = timeStatus();
  if (timeStatus() != timeStatus_old)
  {

    PrintTimeStatus();

    if (timeStatus() == timeSet)
    {
      if (_configTime.enablentp == true)
      {
        int longInterval = getInterval();
        setSyncInterval(longInterval);
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.print(F("Sync interval set to "));
        Serial.print(longInterval);
        Serial.println(F(" seconds"));
      }
    }
    else if (timeStatus() != timeSet)
    {
      if (_configTime.enablentp == true)
      {
        int shortInterval = getShortInterval();
        setSyncInterval(shortInterval);
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.print(F("Sync interval set to "));
        Serial.print(shortInterval);
        Serial.println(F(" seconds"));
      }
    }

    timeStatus_old = timeStatus();
  }

  if (syncNtpEventTriggered || syncRtcEventTriggered)
  {

    // store first sync and last sync
    if (_firstSync == 0)
    {
      if (timeStatus() == timeSet)
      {

        _firstSync = now();
        Serial.print(F("_firstSync: "));
        Serial.println(_firstSync);

        _millisFirstSync = millis();
        Serial.print(F("millisFirstSync: "));
        Serial.println(_millisFirstSync);

        _secondsFirstSync = _millisFirstSync / 1000;
        Serial.print(F("_secondsFirstSync: "));
        Serial.println(_secondsFirstSync);

        _lastBoot = _firstSync - _secondsFirstSync;
        Serial.print(F("_lastBoot: "));
        Serial.println(_lastBoot);
      }
    }

    if (syncNtpEventTriggered)
    {
      if (timeStatus() != timeSet)
      {
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("NTP has failed setting the time"));
      }
      else
      {
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("NTP has succesfully set the time"));
      }
    }

    if (syncRtcEventTriggered)
    {
      if (timeStatus() != timeSet)
      {
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("RTC has failed setting the time"));
      }
      else
      {
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("RTC has succesfully set the time"));
      }
    }

    syncNtpEventTriggered = false;
    syncRtcEventTriggered = false;
  }

  //Select time source
  static boolean NTP_OK_old = NTP_OK;
  static boolean RTC_OK_old = RTC_OK;

  if (NTP_OK != NTP_OK_old || RTC_OK != RTC_OK_old)
  {

    if (NTP_OK != NTP_OK_old)
    {
      if (NTP_OK)
      {
        digitalClockDisplay();
        Serial.println(F(">"));
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("NTP OK"));
      }
      else
      {
        digitalClockDisplay();
        Serial.println(F(">"));
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("NTP NOT OK"));
      }
      // update values
      NTP_OK_old = NTP_OK;
    }

    if (RTC_OK != RTC_OK_old)
    {
      if (RTC_OK)
      {
        digitalClockDisplay();
        Serial.println(F(">"));
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("RTC OK"));
      }
      else
      {
        digitalClockDisplay();
        Serial.println(F(">"));
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("RTC NOT OK"));
      }
      // update values
      RTC_OK_old = RTC_OK;
    }

    digitalClockDisplay();
    Serial.println(F("> "));
    digitalClockDisplay();
    Serial.print(F("> "));
    Serial.print(F("Update selected time source... "));
    if (RTC_OK == true)
    {
      Serial.println(F("RTC"));
      _timeSource = TIMESOURCE_RTC;
    }
    else if (NTP_OK == true && RTC_OK == false)
    {
      Serial.println(F("NTP"));
      _timeSource = TIMESOURCE_NTP;
      // Serial.println(F(", but DONE NOTHING :("));
    }
    // else if (NTP_OK == false && RTC_OK == false) {
    else
    {
      Serial.println(F("*** NO VALID TIME SOURCE IS AVAILABLE ***"));
      _timeSource = TIMESOURCE_NOT_AVAILABLE;
    }
    Serial.println();
  }

  static byte _timeSource_old = _timeSource;

  if (_timeSource != _timeSource_old)
  {

    if (utc_time() - _lastSyncd > 15)
    { // NTP server limit sync frequency to max 15 seconds
      digitalClockDisplay();
      Serial.print(F("> "));
      Serial.println(F("Yay.. last sync is more than 15 seconds ago :-)"));

      if (_timeSource == TIMESOURCE_RTC)
      {
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("Set sync provider to RTC"));
        setSyncProvider(get_time_from_rtc);
      }
      // else if (_timeSource = TIMESOURCE_NTP) {
      else
      {
        digitalClockDisplay();
        Serial.print(F("> "));
        Serial.println(F("Set sync provider to NTP"));
        setSyncProvider(getNtpTimeSDK);
      }

      // update old values only when 15 seconds has elapsed
      _timeSource_old = _timeSource;
    }
  }

  //update the display every 1000ms
  if (tick1000ms)
  {
    CheckNewTimeSettingsReceived();

    if (_configTime.enablertc == true)
    {
      RtcStatusLoop();
      GET_RTC_TEMPERATURE();
    }

    TMP102_loop();

    if (timeStatus() != timeNotSet)
    {
      ProcessSholatEverySecond();

      AlarmTrigger();

      PageAutomaticMode();
    }
  }

  AsyncWSLoop();
  buzzer(BUZZER, _ledMatrixSettings.brightness);
  Tone0(BUZZER, _ledMatrixSettings.brightness);
  Tone1(BUZZER, _ledMatrixSettings.brightness);
  Tone10(BUZZER, duration);
  runningled_loop();
  mqtt_loop();

  tcpCleanup();

  encoder0PosIncreasing = false;
  encoder0PosDecreasing = false;

  tick500ms = false;
  tick1000ms = false;
}