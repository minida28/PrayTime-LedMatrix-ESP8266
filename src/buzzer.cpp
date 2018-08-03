#include "buzzer.h"

bool alarmState;
bool tone0;
bool tone1;
bool tone10;

uint16_t loudestFreq = 2010; //test your buzzer to find its loudest frequency (natural frequency)
uint16_t loudestDuty = 350;

uint16_t duration;

void x_tone(uint8_t pin, uint16_t freq)
{
  //    uint16_t cpuFreq = ESP.getCpuFreqMHz();
  //    uint8_t cpuFreqFactor;
  //    if (cpuFreq == 80) {
  //    cpuFreqFactor = 2;
  //  }
  //  else if (cpuFreq == 160) {
  //    cpuFreqFactor = 1;
  //  }
  //  analogWriteFreq(cpuFreqFactor * freq);
  analogWrite(pin, 512);
}

void buzzer(uint8_t pinBuzzer, uint16_t duty)
{
  if (alarmState == HIGH)
  {
    static byte buzzerState;

    unsigned long timeBuzzerState1 = 100;
    unsigned long timeBuzzerState2 = 100;
    unsigned long timeBuzzerState3 = 100;
    unsigned long timeBuzzerState4 = 100;
    // unsigned long timeBuzzerON = 50;
    unsigned long timeBuzzerOFF = 1000 - 4 * 100;

    //    int freq_0 = 4435; //2637
    //    int freq_1 = 5274; //2217
    unsigned int freq_0 = 2217; //4699
    unsigned int freq_1 = 2637; //5588

    static unsigned long previousMillisBuzzer;

    if (buzzerState == 0 && millis() - previousMillisBuzzer >= timeBuzzerOFF)
    {
      buzzerState = 1;                 // Update the state
      tone(BUZZER, freq_1);            // Turn on Solenoid Valve
      previousMillisBuzzer = millis(); // Remember the time
      //Serial.println(buzzerState);
    }
    if (buzzerState == 1 && millis() - previousMillisBuzzer >= timeBuzzerState1)
    {
      buzzerState = 2;                 // Update the state
      tone(BUZZER, freq_0);            // Turn on Solenoid Valve
      previousMillisBuzzer = millis(); // Remember the time
      //Serial.println(buzzerState);
    }
    if (buzzerState == 2 && millis() - previousMillisBuzzer >= timeBuzzerState2)
    {
      buzzerState = 3;                 // Update the state
      tone(BUZZER, freq_1);            // Turn on Solenoid Valve
      previousMillisBuzzer = millis(); // Remember the time
      //Serial.println(buzzerState);
    }
    if (buzzerState == 3 && millis() - previousMillisBuzzer >= timeBuzzerState3)
    {
      buzzerState = 4;                 // Update the state
      tone(BUZZER, freq_0);            // Turn on Solenoid Valve
      previousMillisBuzzer = millis(); // Remember the time
      //Serial.println(buzzerState);
    }
    if (buzzerState == 4 && millis() - previousMillisBuzzer >= timeBuzzerState4)
    {
      buzzerState = 0;                 // Update the state
      noTone(BUZZER);                  // Turn on Solenoid Valve
      previousMillisBuzzer = millis(); // Remember the time
      //Serial.println(buzzerState);
      alarmState = LOW;
    }
  }
}

//void buzzer(uint8_t pinBuzzer, uint16_t duty)
//{
//  duty = loudestDuty;
//  int freq_0 = loudestFreq; //5588; //2217 // 2000
//  int freq_1 = 1690; //4699 ; //2637 //1682
//
//  if (alarmState == HIGH)
//  {
//    static byte buzzerState;
//
//    unsigned long timeBuzzerState1 = 100;
//    unsigned long timeBuzzerState2 = 100;
//    unsigned long timeBuzzerState3 = 100;
//    unsigned long timeBuzzerState4 = 100;
//    unsigned long timeBuzzerON = 50;
//    unsigned long timeBuzzerOFF = 1000 - 4 * 100;
//
//
//
//    static unsigned long previousMillisBuzzer;
//
//    if (buzzerState == 0 && millis() - previousMillisBuzzer >= timeBuzzerOFF)
//    {
//      buzzerState = 1; // Update the state
//      analogWriteFreq(freq_0);
//      analogWrite(pinBuzzer, duty);
//      previousMillisBuzzer = millis(); // Remember the time
//      //Serial.println(buzzerState);
//    }
//    if (buzzerState == 1 && millis() - previousMillisBuzzer >= timeBuzzerState1)
//    {
//      buzzerState = 2; // Update the state
//      analogWriteFreq(freq_1);
//      analogWrite(pinBuzzer, duty);
//      previousMillisBuzzer = millis(); // Remember the time
//      //Serial.println(buzzerState);
//    }
//    if (buzzerState == 2 && millis() - previousMillisBuzzer >= timeBuzzerState2)
//    {
//      buzzerState = 3; // Update the state
//      analogWriteFreq(freq_0);
//      analogWrite(pinBuzzer, duty);
//      previousMillisBuzzer = millis(); // Remember the time
//      //Serial.println(buzzerState);
//    }
//    if (buzzerState == 3 && millis() - previousMillisBuzzer >= timeBuzzerState3)
//    {
//      buzzerState = 4; // Update the state
//      analogWriteFreq(freq_1);
//      analogWrite(pinBuzzer, duty);
//      previousMillisBuzzer = millis(); // Remember the time
//      //Serial.println(buzzerState);
//    }
//    if (buzzerState == 4 && millis() - previousMillisBuzzer >= timeBuzzerState4)
//    {
//      buzzerState = 0; // Update the state
//      noTone(pinBuzzer);          // Turn on Solenoid Valve
//      analogWriteFreq(ANALOGWRITE_OPERATING_FREQUENCY);
//      previousMillisBuzzer = millis(); // Remember the time
//      //Serial.println(buzzerState);
//      alarmState = LOW;
//    }
//  }
//}

//void Tone0(uint8_t pinBuzzer, uint16_t duty)
//{
//  static bool sound;
//  static unsigned long startMillis;
//  if (tone0) {
//    tone0 = false;
//    startMillis = millis();
//
//    //tone(pin, 8000);
//    analogWriteFreq(loudestFreq);
//    analogWrite(pinBuzzer, loudestDuty);
//
//    sound = true;
//  }
//  if (sound && millis() - startMillis >= 50) {
//    noTone(pinBuzzer);
//    analogWriteFreq(ANALOGWRITE_OPERATING_FREQUENCY);
//    sound = false;
//  }
//}

void Tone0(uint8_t pinBuzzer, uint16_t duty)
{
  static bool sound;
  static unsigned long startMillis;
  unsigned int freq = 4000;
  // if (tone0) {
  //   tone0 = false;
  //   startMillis = millis();
  //   tone(pinBuzzer, freq);
  //   sound = true;
  // }
  // if (sound && millis() - startMillis >= 50) {
  //   noTone(pinBuzzer);
  //   sound = false;
  // }

  if (tone0)
  {
    tone0 = false;
    tone(pinBuzzer, freq, 50);
  }
}

// void Tone0(uint8_t pinBuzzer, uint16_t duty)
// {
//   unsigned int freq = 8000;
//   unsigned long dur = 50;
//   tone(pinBuzzer, freq, dur);
// }

void Tone1(uint8_t pinBuzzer, uint16_t duty)
{
  if (tone1)
  {

    unsigned int freq = 4000;

    tone(pinBuzzer, freq);
    delay(80);
    noTone(pinBuzzer);
    delay(75);
    tone(pinBuzzer, freq);
    delay(50);
    noTone(pinBuzzer);
    delay(25);
    tone(pinBuzzer, freq);
    delay(50);
    noTone(pinBuzzer);

    //    analogWriteFreq(loudestFreq);
    //
    //    analogWrite(pinBuzzer, loudestDuty);
    //    delay(80);
    //    noTone(pinBuzzer);
    //    delay(75);
    //    analogWrite(pinBuzzer, loudestDuty);
    //    delay(50);
    //    noTone(pinBuzzer);
    //    delay(25);
    //    analogWrite(pinBuzzer, loudestDuty);
    //    delay(50);
    //    noTone(pinBuzzer);
    //
    //    analogWriteFreq(ANALOGWRITE_OPERATING_FREQUENCY);

    tone1 = false;
  }
}

//void Tone10(uint8_t pin, uint16_t duration)
//{
//  static time_t start;
//  static bool state;
//  if (tone10) {
//    pinMode(pin, OUTPUT);
//    if (state == LOW) {
//      start = millis();
//      state = HIGH;
//    }
//    if (state == HIGH && millis() - start >= duration) {
//      state = LOW;
//      tone10 = false;
//    }
//    digitalWrite(pin, state);
//  }
//}

//void Tone10(uint8_t pin, uint16_t duration)
//{
//  uint16_t duty = loudestDuty;
//  uint16_t freq = loudestFreq;
//
//  static time_t start;
//  static bool state;
//  if (tone10) {
//    if (state == LOW) {
//      start = millis();
//      state = HIGH;
//      analogWriteFreq(freq);
//      analogWrite(pin, duty);
//    }
//    if (state == HIGH && millis() - start >= duration) {
//      state = LOW;
//      noTone(pin);
//      analogWriteFreq(ANALOGWRITE_OPERATING_FREQUENCY);
//      tone10 = false;
//    }
//    //digitalWrite(pin, state);
//  }
//}

void Tone10(uint8_t pinBuzzer, uint16_t duration)
{
  static time_t start;
  static bool state = LOW;
  unsigned int freq = 2375; //4750
  if (tone10)
  {
    if (state == LOW)
    {
      start = millis();
      state = HIGH;
      tone(pinBuzzer, freq);
      //tone(pinBuzzer, 440);
    }
    if (state == HIGH && millis() - start >= duration)
    {
      state = LOW;
      noTone(pinBuzzer);
      tone10 = false;
    }
  }
}

void BuzzerSetup()
{
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  pinMode(LED_1, OUTPUT);
  digitalWrite(LED_1, LOW);
}
