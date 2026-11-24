/*!
 * @file SSD1306.h
 *
 * Slim customized version of Adafruit's SSD1306 library for monochrome
 * OLED displays using direct AVR TWI (TWI_Master) without Wire/Adafruit_GFX.
 *
 * BSD license (original Adafruit_SSD1306 by Limor Fried/Ladyada).
 */

#ifndef DOOM_MEGA_SSD1306_H
#define DOOM_MEGA_SSD1306_H

#include <stdint.h>
#include <string.h>
#include "TWI_Master.h"
#include "constants.h"

#ifndef NO_ADAFRUIT_SSD1306_COLOR_COMPATIBILITY
#define BLACK                     SSD1306_BLACK
#define WHITE                     SSD1306_WHITE
#define INVERSE                   SSD1306_INVERSE
#endif

#define SSD1306_BLACK               0
#define SSD1306_WHITE               1
#define SSD1306_INVERSE             2

#define SSD1306_MEMORYMODE          0x20
#define SSD1306_COLUMNADDR          0x21
#define SSD1306_PAGEADDR            0x22
#define SSD1306_SETCONTRAST         0x81
#define SSD1306_CHARGEPUMP          0x8D
#define SSD1306_SEGREMAP            0xA0
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON        0xA5
#define SSD1306_NORMALDISPLAY       0xA6
#define SSD1306_INVERTDISPLAY       0xA7
#define SSD1306_SETMULTIPLEX        0xA8
#define SSD1306_DISPLAYOFF          0xAE
#define SSD1306_DISPLAYON           0xAF
#define SSD1306_COMSCANINC          0xC0
#define SSD1306_COMSCANDEC          0xC8
#define SSD1306_SETDISPLAYOFFSET    0xD3
#define SSD1306_SETDISPLAYCLOCKDIV  0xD5
#define SSD1306_SETPRECHARGE        0xD9
#define SSD1306_SETCOMPINS          0xDA
#define SSD1306_SETVCOMDETECT       0xDB

#define SSD1306_SETLOWCOLUMN        0x00
#define SSD1306_SETHIGHCOLUMN       0x10
#define SSD1306_SETSTARTLINE        0x40

#define SSD1306_EXTERNALVCC         0x01
#define SSD1306_SWITCHCAPVCC        0x02

#define SSD1306_RIGHT_HORIZONTAL_SCROLL              0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL               0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL  0x2A
#define SSD1306_DEACTIVATE_SCROLL                    0x2E
#define SSD1306_ACTIVATE_SCROLL                      0x2F
#define SSD1306_SET_VERTICAL_SCROLL_AREA             0xA3

template <uint8_t WIDTH, uint8_t HEIGHT>
class Adafruit_SSD1306 {
 public:
  Adafruit_SSD1306() = default;
  ~Adafruit_SSD1306(void) = default;

  bool     begin(uint8_t switchvcc = SSD1306_SWITCHCAPVCC,
                 uint8_t i2caddr = OLED_I2C_ADDR);
  void     display(void);
  void     clearDisplay(void);
  void     invertDisplay(bool i);
  void     drawPixel(int16_t x, int16_t y, uint16_t color);
  void     drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  bool     getPixel(int16_t x, int16_t y);
  uint8_t *getBuffer(void);
  void     clearRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
  void     drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color);

 private:
  void     drawFastVLineInternal(int16_t x, int16_t y, int16_t h, uint16_t color);
  void     ssd1306_command1(uint8_t c);
  void     ssd1306_commandList(const uint8_t *c, uint8_t n);

  uint8_t  buffer[WIDTH * ((HEIGHT + 7) / 8)];
  uint8_t  i2caddr = OLED_I2C_ADDR;
  uint8_t  vccstate = SSD1306_SWITCHCAPVCC;
};

extern template class Adafruit_SSD1306<SCREEN_WIDTH, SCREEN_HEIGHT>;

#endif // DOOM_MEGA_SSD1306_H
