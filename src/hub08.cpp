/*********************************************************************
  This is a library for our Monochrome OLEDs based on SSD1306 drivers
  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/category/63_98
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!
  Written by Limor Fried/Ladyada  for Adafruit Industries.
  BSD license, check license.txt for more information
  All text above, and the splash screen below must be included in any redistribution
*********************************************************************/

#ifdef __AVR__
#include <avr/pgmspace.h>
#elif defined(ESP8266)
#include <pgmspace.h>
#else
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif

#if !defined(__ARM_ARCH) && !defined(ENERGIA) && !defined(ESP8266)
#include <util/delay.h>
#endif

#include <stdlib.h>

#include <Wire.h>
//#include <SPI.h>
//#include "Adafruit_GFX.h"
#include "HUB08.h"
#include "displayhelper.h"

#if 0
#define ASSERT(e)   if (!(e)) { Serial.println(#e); while (1); }
#else
#define ASSERT(e)
#endif

//#define digitalWrite(oe, HIGH) pcf8574.write(7, LOW)
//#define digitalWrite(oe, LOW) pcf8574.write(7, HIGH)

// the memory buffer for the LCD

//static uint8_t buffer[HUB08_LCDHEIGHT * HUB08_LCDWIDTH / 8] = {
//  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//  0x01, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x01, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60,
//  0x01, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0xE0, 0x07, 0x8F, 0xC7, 0xC7, 0xC7, 0xE0,
//  0x00, 0x40, 0x0C, 0xCE, 0x6C, 0x6C, 0x6C, 0xE0, 0x00, 0xE0, 0x0C, 0x0C, 0x6C, 0x6C, 0x6C, 0x60,
//  0x01, 0xF0, 0x07, 0x8C, 0x6F, 0xEF, 0xEC, 0x60, 0x23, 0xF8, 0x00, 0xCC, 0x6C, 0x0C, 0x0C, 0x60,
//  0x33, 0xF8, 0x0C, 0xCE, 0x6C, 0x6C, 0x6C, 0xE0, 0x3B, 0xF8, 0x07, 0x8F, 0xC7, 0xC7, 0xC7, 0xE0,
//  0x3B, 0xF8, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x1B, 0xF8, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00,
//  0x0B, 0xF8, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x07, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
//};

uint8_t buffer[HUB08_LCDHEIGHT * HUB08_LCDWIDTH / 8];
//uint8_t buffer[BUFFER_HEIGHT * BUFFER_WIDTH / 8];



// the most basic function, set a single pixel
void HUB08::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x < 0) || (x >= width()) || (y < 0) || (y >= height()))
    return;

  uint8_t *byte = buffer + x / 8 + y * width() / 8;
  uint8_t  bit = x % 8;

  if (color) {
    *byte |= 0x80 >> bit;
  } else {
    *byte &= ~(0x80 >> bit);
  }
  /*
    // x is which column
    switch (color)
    {
    case WHITE:   buffer[x+ (y/8)*HUB08_LCDWIDTH] |=  (1 << (y&7)); break;
    case BLACK:   buffer[x+ (y/8)*HUB08_LCDWIDTH] &= ~(1 << (y&7)); break;
    case INVERSE: buffer[x+ (y/8)*HUB08_LCDWIDTH] ^=  (1 << (y&7)); break;
    }
  */
}

//void HUB08::drawPixel(int16_t x, int16_t y, uint16_t pixel)
//{
//  ASSERT(width() > x);
//  ASSERT(height() > y);
//
//  uint8_t *byte = buffer + x / 8 + y * width() / 8;
//  uint8_t  bit = x % 8;
//
//  if (pixel) {
//    *byte |= 0x80 >> bit;
//  } else {
//    *byte &= ~(0x80 >> bit);
//  }
//}


HUB08::HUB08(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t oe, uint8_t r1, uint8_t stb, uint8_t clk) :
  Adafruit_GFX(HUB08_LCDWIDTH, HUB08_LCDHEIGHT) {
  //Adafruit_GFX(HUB08_LCDHEIGHT, HUB08_LCDWIDTH) {
  this->clk = clk;
  this->r1 = r1;
  this->stb = stb;
  this->oe = oe;
  this->a = a;
  this->b = b;
  this->c = c;
  this->d = d;

  //mask = 0xff;
  mask = 0x00;
  state = 0;
}


void HUB08::begin()
{
  ASSERT(0 == (width() % 32));
  ASSERT(0 == (height() % 16));

#ifndef USE_IO_EXPANDER
  pinMode(a, OUTPUT);
  pinMode(b, OUTPUT);
  pinMode(c, OUTPUT);
  pinMode(d, OUTPUT);
#endif
  pinMode(oe, OUTPUT);
  pinMode(r1, OUTPUT);
  pinMode(stb, OUTPUT);
  pinMode(clk, OUTPUT);

  //analogWrite(oe, 800);

  state = 1;
}


void HUB08::scan()
{
  static uint8_t row = 0;  // from 0 to 15

  if (!state) {
    return;
  }

  uint8_t *head = buffer + row * (width() / 8);
  for (uint8_t line = 0; line < (height() / 16); line++) {

    uint8_t *ptr = head;
    head += width() * 2;              // width * 16 / 8

    for (uint8_t byte = 0; byte < (width() / 8); byte++) {
      uint8_t pixels = *ptr;
      ptr++;
      pixels = pixels ^ mask;     // reverse: mask = 0xff, normal: mask =0x00
      for (uint8_t bit = 0; bit < 8; bit++) {
        digitalWrite(clk, LOW);
        digitalWrite(r1, pixels & (0x80 >> bit));
        digitalWrite(clk, HIGH);
      }
    }
  }

  digitalWrite(oe, HIGH);              // disable display
  //analogWrite(oe, _ledMatrixSettings.brightness);

#ifndef USE_IO_EXPANDER
  digitalWrite(a, (row & 0x01));
  digitalWrite(b, (row & 0x02));
  digitalWrite(c, (row & 0x04));
  digitalWrite(d, (row & 0x08));
#else
  // pin P7
  uint8_t maskA = B11111111;
  if (row & 0x01) {
    //do nothing
  }
  else {
    bitClear(maskA, a); //maskA become B01111111;
  }

  // pin P6
  uint8_t maskB = B11111111;
  if (row & 0x02) {
    //do nothing
  }
  else {
    bitClear(maskB, b); //maskB become B10111111;
  }

  // pin P5
  uint8_t maskC = B11111111;
  if (row & 0x04) {
    //do nothing
  }
  else {
    bitClear(maskC, c); //maskC become B11011111;
  }

  // pin P4
  uint8_t maskD = B11111111;
  if (row & 0x08) {
    //do nothing
  }
  else {
    bitClear(maskD, d); //maskD become B11101111;
  }

  uint8_t temp = maskA & maskB & maskC & maskD;

  Wire.beginTransmission(IO_EXPANDER_ADDRESS);
  Wire.write(temp);
  Wire.endTransmission();
  
  //ioexp.write8(temp);

#endif

  // latch data
  digitalWrite(stb, LOW);
  digitalWrite(stb, HIGH);
  digitalWrite(stb, LOW);

  digitalWrite(oe, LOW);              // enable display

  //analogWrite(oe, _ledMatrixSettings.brightness);
  delayMicroseconds(_ledMatrixSettings.brightness);
  digitalWrite(oe, HIGH);

  row = (row + 1) & 0x0F;
}

void HUB08::on()
{
  state = 1;
  //digitalWrite(oe, LOW);
}

void HUB08::off()
{
  state = 0;
  digitalWrite(oe, HIGH);
}

// clear everything
void HUB08::clearDisplay(void) {
  memset(buffer, 0, (HUB08_LCDWIDTH * HUB08_LCDHEIGHT / 8));
}

void HUB08::copyBuffer(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t buf[]) {

  uint16_t *ptr = buf;

  /* Default copy left to right */
  int16_t x_offset = 0;
  int16_t x_stop   = w;
  // int16_t x_dir    = 1;
  /* Default copy top to bottom */
  int16_t y_offset = 0;
  int16_t y_stop   = h;
  // int16_t y_dir    = 1;

  uint16_t index = 0;
  for (y_offset = 0; y_offset < y_stop; y_offset++) {
    if (x_offset == x_stop) {
      x_offset = 0;
    }
    for (x_offset = 0; x_offset < x_stop; x_offset++) {
      ptr[index] = getPixel(x + x_offset, y + y_offset);
      index++;
    }
  }
  ptr[index] = '\0';
  // unsigned int origin, destination;
  // origin = (y + h - 1) * width() + x;
  // destination = (y + h) * width() + x;
}

uint16_t HUB08::getPixel(int16_t x, int16_t y) {
  unsigned int index = (y * width() + x) / 8;
  int remainder = x % 8; //modulo
  uint16_t pixel = bitRead(buffer[index], 7 - remainder);
  return pixel;
}

void HUB08::reverse()
{
  mask = ~mask;
}

uint8_t HUB08::isReversed()
{
  return mask;
}

void HUB08::invertDisplay(boolean i)
{
  if (i) {
    mask = ~mask;
  }
}





