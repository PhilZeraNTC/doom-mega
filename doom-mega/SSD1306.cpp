/*!
 * @file SSD1306.cpp
 *
 * Slim customized version of Adafruit's SSD1306 library for monochrome
 * OLED displays using direct AVR TWI (TWI_Master) without Wire/Adafruit_GFX.
 *
 * BSD license (original Adafruit_SSD1306 by Limor Fried/Ladyada).
 */

#ifdef __AVR__
 #include <avr/pgmspace.h>
#elif defined(ESP8266) || defined(ESP32)
 #include <pgmspace.h>
#else
 #define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif

#include "SSD1306.h"

constexpr uint16_t WIRE_MAX = 342;
constexpr uint8_t  CMD_LIST_CHUNK_SIZE = 16;

// LOW-LEVEL UTILS ---------------------------------------------------------

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::ssd1306_command1(uint8_t c) {
  uint8_t cmd = 0x00; // Co = 0, D/C = 0
  TWI_Start_Transceiver_With_Data(cmd, &c, 1);
}

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::ssd1306_commandList(const uint8_t *c, uint8_t n) {
  uint8_t cmd = 0x00; // Co = 0, D/C = 0
  uint8_t tmp[CMD_LIST_CHUNK_SIZE];
  uint8_t offset = 0;

  while (offset < n) {
    uint8_t remaining = n - offset;
    uint8_t chunk = (remaining > CMD_LIST_CHUNK_SIZE) ? CMD_LIST_CHUNK_SIZE : remaining;
    memcpy_P(tmp, c + offset, chunk);
    TWI_Start_Transceiver_With_Data(cmd, tmp, chunk);
    offset += chunk;
  }
}

// ALLOCATE & INIT DISPLAY -------------------------------------------------

template <uint8_t WIDTH, uint8_t HEIGHT>
bool Adafruit_SSD1306<WIDTH, HEIGHT>::begin(uint8_t vcs, uint8_t addr) {
  clearDisplay();

  vccstate = vcs;
  i2caddr = addr ? addr : ((HEIGHT == 32) ? 0x3C : 0x3D);
  (void)i2caddr; // TWI_Master uses compile-time TWI_ADDR derived from OLED_I2C_ADDR

  TWI_Master_Initialise();

  // Init sequence
  static const uint8_t PROGMEM init1[] = {
    SSD1306_DISPLAYOFF,                   // 0xAE
    SSD1306_SETDISPLAYCLOCKDIV,           // 0xD5
    0x80,                                 // suggested ratio 0x80
    SSD1306_SETMULTIPLEX };               // 0xA8
  ssd1306_commandList(init1, sizeof(init1));
  ssd1306_command1(HEIGHT - 1);

  static const uint8_t PROGMEM init2[] = {
    SSD1306_SETDISPLAYOFFSET,             // 0xD3
    0x00,                                 // no offset
    SSD1306_SETSTARTLINE | 0x0,           // line #0
    SSD1306_CHARGEPUMP };                 // 0x8D
  ssd1306_commandList(init2, sizeof(init2));

  ssd1306_command1((vccstate == SSD1306_EXTERNALVCC) ? 0x10 : 0x14);

  static const uint8_t PROGMEM init3[] = {
    SSD1306_MEMORYMODE,                   // 0x20
    0x00,                                 // horizontal addressing mode
    SSD1306_SEGREMAP | 0x1,
    SSD1306_COMSCANDEC };
  ssd1306_commandList(init3, sizeof(init3));

  if ((WIDTH == 128) && (HEIGHT == 32)) {
    static const uint8_t PROGMEM init4a[] = {
      SSD1306_SETCOMPINS,                 // 0xDA
      0x02,
      SSD1306_SETCONTRAST,                // 0x81
      0x8F };
    ssd1306_commandList(init4a, sizeof(init4a));
  } else if ((WIDTH == 128) && (HEIGHT == 64)) {
    static const uint8_t PROGMEM init4b[] = {
      SSD1306_SETCOMPINS,                 // 0xDA
      0x12,
      SSD1306_SETCONTRAST };              // 0x81
    ssd1306_commandList(init4b, sizeof(init4b));
    ssd1306_command1((vccstate == SSD1306_EXTERNALVCC) ? 0x9F : 0xCF);
  } else if ((WIDTH == 96) && (HEIGHT == 16)) {
    static const uint8_t PROGMEM init4c[] = {
      SSD1306_SETCOMPINS,                 // 0xDA
      0x02,
      SSD1306_SETCONTRAST };              // 0x81
    ssd1306_commandList(init4c, sizeof(init4c));
    ssd1306_command1((vccstate == SSD1306_EXTERNALVCC) ? 0x10 : 0xAF);
  }

  ssd1306_command1(SSD1306_SETPRECHARGE); // 0xD9
  ssd1306_command1((vccstate == SSD1306_EXTERNALVCC) ? 0x22 : 0xF1);

  static const uint8_t PROGMEM init5[] = {
    SSD1306_SETVCOMDETECT,                // 0xDB
    0x40,
    SSD1306_DISPLAYALLON_RESUME,          // 0xA4
    SSD1306_NORMALDISPLAY,                // 0xA6
    SSD1306_DEACTIVATE_SCROLL,
    SSD1306_DISPLAYON };                  // Main screen turn on
  ssd1306_commandList(init5, sizeof(init5));

  return true;
}

// DRAWING FUNCTIONS -------------------------------------------------------

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x >= 0) && (x < WIDTH) && (y >= 0) && (y < HEIGHT)) {
    switch (color) {
      case SSD1306_WHITE:   buffer[x + (y / 8) * WIDTH] |=  (1 << (y & 7)); break;
      case SSD1306_BLACK:   buffer[x + (y / 8) * WIDTH] &= ~(1 << (y & 7)); break;
      case SSD1306_INVERSE: buffer[x + (y / 8) * WIDTH] ^=  (1 << (y & 7)); break;
    }
  }
}

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::clearDisplay(void) {
  memset(buffer, 0, WIDTH * ((HEIGHT + 7) / 8));
}

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::drawFastVLine(
  int16_t x, int16_t y, int16_t h, uint16_t color) {
  drawFastVLineInternal(x, y, h, color);
}

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::drawFastVLineInternal(
  int16_t x, int16_t __y, int16_t __h, uint16_t color) {

  if ((x >= 0) && (x < WIDTH)) {
    if (__y < 0) {
      __h += __y;
      __y = 0;
    }
    if ((__y + __h) > HEIGHT) {
      __h = (HEIGHT - __y);
    }
    if (__h > 0) {
      uint8_t  y = (uint8_t)__y, h = (uint8_t)__h;
      uint8_t *pBuf = &buffer[(y / 8) * WIDTH + x];

      uint8_t mod = (y & 7);
      if (mod) {
        mod = 8 - mod;
        static const uint8_t PROGMEM premask[8] =
          { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
        uint8_t mask = pgm_read_byte(&premask[mod]);
        if (h < mod) mask &= (0xFF >> (mod - h));

        switch (color) {
          case SSD1306_WHITE:   *pBuf |=  mask; break;
          case SSD1306_BLACK:   *pBuf &= ~mask; break;
          case SSD1306_INVERSE: *pBuf ^=  mask; break;
        }
        pBuf += WIDTH;
      }

      if (h >= mod) {
        h -= mod;
        if (h >= 8) {
          if (color == SSD1306_INVERSE) {
            do {
              *pBuf ^= 0xFF;
              pBuf  += WIDTH;
              h     -= 8;
            } while (h >= 8);
          } else {
            uint8_t val = (color != SSD1306_BLACK) ? 255 : 0;
            do {
              *pBuf = val;
              pBuf += WIDTH;
              h    -= 8;
            } while (h >= 8);
          }
        }

        if (h) {
          mod = h & 7;
          static const uint8_t PROGMEM postmask[8] =
            { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F };
          uint8_t mask = pgm_read_byte(&postmask[mod]);
          switch (color) {
            case SSD1306_WHITE:   *pBuf |=  mask; break;
            case SSD1306_BLACK:   *pBuf &= ~mask; break;
            case SSD1306_INVERSE: *pBuf ^=  mask; break;
          }
        }
      }
    }
  }
}

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::clearRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  for (int16_t i = x; i < (int16_t)x + (int16_t)w; i++) {
    drawFastVLineInternal(i, y, h, 0);
  }
}

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::drawBitmap(int16_t x, int16_t y,
  const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color) {

  int16_t byteWidth = (w + 7) / 8;
  uint8_t byte = 0;

  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7) byte <<= 1;
      else       byte   = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      if (byte & 0x80) drawPixel(x + i, y, color);
    }
  }
}

template <uint8_t WIDTH, uint8_t HEIGHT>
bool Adafruit_SSD1306<WIDTH, HEIGHT>::getPixel(int16_t x, int16_t y) {
  if ((x >= 0) && (x < WIDTH) && (y >= 0) && (y < HEIGHT)) {
    return (buffer[x + (y / 8) * WIDTH] & (1 << (y & 7))) != 0;
  }
  return false;
}

template <uint8_t WIDTH, uint8_t HEIGHT>
uint8_t *Adafruit_SSD1306<WIDTH, HEIGHT>::getBuffer(void) {
  return buffer;
}

// REFRESH DISPLAY ---------------------------------------------------------

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::display(void) {
  static const uint8_t PROGMEM dlist1[] = {
    SSD1306_PAGEADDR,
    0,                         // Page start address
    0xFF,                      // Page end
    SSD1306_COLUMNADDR,
    0 };                       // Column start address
  ssd1306_commandList(dlist1, sizeof(dlist1));
  ssd1306_command1(WIDTH - 1); // Column end address

  uint16_t count = WIDTH * ((HEIGHT + 7) / 8);
  uint8_t *ptr   = buffer;
  uint8_t  cmd   = 0x40;
  while (count >= WIRE_MAX) {
    TWI_Start_Transceiver_With_Data(cmd, ptr, WIRE_MAX);
    count -= WIRE_MAX;
    ptr   += WIRE_MAX;
  }
  if (count > 0) {
    TWI_Start_Transceiver_With_Data(cmd, ptr, count);
  }
}

// OTHER HARDWARE SETTINGS -------------------------------------------------

template <uint8_t WIDTH, uint8_t HEIGHT>
void Adafruit_SSD1306<WIDTH, HEIGHT>::invertDisplay(bool i) {
  ssd1306_command1(i ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
}

// Explicit template instantiation after all member definitions
template class Adafruit_SSD1306<SCREEN_WIDTH, SCREEN_HEIGHT>;
