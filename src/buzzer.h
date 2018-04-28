#ifndef mybuzzer_h
#define mybuzzer_h

//#include <stdint.h>
#include <Arduino.h>
#include "config.h"


#define PINBUZZER_2 15 //D8



extern bool alarmState;
extern byte buzzerState;
extern bool tone0;
extern bool tone1;
extern bool tone10;

extern uint16_t duration;

void buzzer(uint8_t pinBuzzer, uint16_t duty);
void Tone0(uint8_t pinBuzzer, uint16_t duty);
void Tone1(uint8_t pinBuzzer, uint16_t duty);
void Tone10(uint8_t pin, uint16_t duration);

void x_tone(uint8_t pin, uint16_t freq);

#endif



