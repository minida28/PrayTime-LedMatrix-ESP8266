/**
   The MIT License (MIT)

   Copyright (c) 2016 by Daniel Eichhorn
   Copyright (c) 2016 by Fabrice Weinberg

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "sholat.h"
#include "sholathelper.h"
#include "timehelper.h"
#include "rtchelper.h"
#include "displayhelper.h"
#include "asyncserver.h"
// #include <Ticker.h>
#include <StreamString.h>
#include "buzzer.h"
#include "mqtt.h"
#include "pinouthelper.h"
#include "ioexpanderhelper.h"
#include "PingAlive.h"

#define DEBUGPORT Serial

// #define RELEASE

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

bool syncNtpToRtcFlag = false;
// bool tick100ms = false;
// bool tick3000ms = false;

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

void setup()
{
  Serial.begin(115200);
  Serial.println();

  BuzzerSetup();

  Serial.println(F("Mounting FS..."));
  if (!SPIFFS.begin())
  {
    Serial.println(F("Failed to mount file system"));
    return;
  }

  mqtt_setup();
  
  AsyncWSBegin();

  DEBUGLOGLN("IP number assigned by DHCP is %s", WiFi.localIP().toString().c_str());

  // -------------------------------------------------------------------
  // Setup I2C stuffs
  // -------------------------------------------------------------------
  DEBUGLOGLN("Clearing I2C Bus"); //http://www.forward.com.au/pfod/ArduinoProgramming/I2C_ClearBus/index.html
  int rtn = I2C_ClearBus();       // clear the I2C bus first before calling Wire.begin()
  if (rtn != 0)
  {
    DEBUGLOGLN("I2C bus error. Could not clear");
    if (rtn == 1)
    {
      DEBUGLOGLN("SCL clock line held low");
    }
    else if (rtn == 2)
    {
      DEBUGLOGLN("SCL clock line held low by slave clock stretch");
    }
    else if (rtn == 3)
    {
      DEBUGLOGLN("SDA data line held low");
    }
  }
  else
  {
    DEBUGLOGLN("bus clear, re-enable Wire.begin();");
    Wire.begin(SDA, SCL);
  }

  // -------------------------------------------------------------------
  // Setup PCF8574
  // -------------------------------------------------------------------
  IoExpanderSetup();

  DisplaySetup();

  RtcSetup();

  TimeSetup();

  //  AsyncWSBegin();

  Serial.println(F("Setup completed\r\n"));
}

void pingFault(void) {}

void loop()
{
  static bool beginPingAlive = true;
  if (WiFi.status() == WL_CONNECTED && beginPingAlive)
  {
    startPingAlive();
    beginPingAlive = false;
  }

  TimeLoop();

  IoExpanderLoop();

  static boolean stateSwitch_old;
  static uint32_t timerSwitch;

  //stateSwitch = toggleSwitch.Update();

  if (stateSwitch != stateSwitch_old)
  {
    stateSwitch_old = stateSwitch;

    if (stateSwitch == HIGH)
    {
      Serial.println(F("Switch ON"));
      timerSwitch = millis();
    }
    else if (stateSwitch == LOW)
    {
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

      if (count >= 4)
      {

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
      }
    }
  }
  else if (stateSwitch == LOW)
  {
    count = 0;
  }

  if (encoder0PosIncreasing || encoder0PosDecreasing)
  {
    Serial.print(F("Encoder Pos: "));
    Serial.println(encoder0Pos);
    tone0 = HIGH;
  }

  // SholatLoop();

  static bool state500ms_old = 0;
  bool tick500ms = false;
  if (state500ms_old != state500ms)
  {
    state500ms_old = state500ms;
    tick500ms = true;
  }

  // static bool updateSholat = false;

  if (timeSetFlag & tick1000ms)
  {
    timeSetFlag = false;

    lastSync = now;
    process_sholat();
    process_sholat_2nd_stage();
  }

  if (tick1000ms)
  {
    // update sholat must be below / after updating the timestamp
    SholatLoop();
    AlarmTrigger();

    PageAutomaticMode();
    // DEBUGLOG("case: %d\r\n", PageAutomaticMode());
  }

  if (tick3000ms)
  {
    rtcTemperature = GetRtcTemp();
  }

  AsyncWSLoop();
  buzzer(BUZZER, _ledMatrixSettings.brightness);
  Tone0(BUZZER, _ledMatrixSettings.brightness);
  Tone1(BUZZER, _ledMatrixSettings.brightness);
  Tone10(BUZZER, duration);
  DisplayLoop();
  mqtt_loop();

  encoder0PosIncreasing = false;
  encoder0PosDecreasing = false;

  tick500ms = false;
  tick1000ms = false;
  tick3000ms = false;
  // timeSetFlag = false;
}