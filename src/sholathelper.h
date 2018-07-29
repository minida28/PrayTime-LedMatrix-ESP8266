#ifndef sholathelper_h
#define sholathelper_h

#include <Arduino.h>
#include "sholat.h"
// #include "sntphelper.h"
#include "timehelper.h"
#include "progmemmatrix.h"

extern PrayerTimes sholat;

extern uint8_t HOUR;
extern uint8_t MINUTE;
extern uint8_t SECOND;
extern uint8_t ceilHOUR;
extern uint8_t ceilMINUTE;
extern uint8_t CURRENTTIMEID, NEXTTIMEID;

extern char bufHOUR[3];
extern char bufMINUTE[3];
extern char bufSECOND[3];

extern char sholatTimeYesterdayArray[TimesCount][6];
extern char sholatTimeArray[TimesCount][6];
extern char sholatTimeTomorrowArray[TimesCount][6];

char* sholatNameStr(uint8_t id);
void process_sholat();
void process_sholat_2nd_stage();
void ProcessSholatEverySecond();

void SholatLoop();

#endif