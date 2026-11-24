#include <Arduino.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <string.h>
#include "display.h"
#include "pinout.h"
#include "sprites.h"

// Reads a char from an F() string
#define F_char(ifsh, ch)    pgm_read_byte(reinterpret_cast<PGM_P>(ifsh) + (ch))

// Bit masks from left to right (slightly faster than bitRead)
static const uint8_t PROGMEM bit_mask[8] = { 128, 64, 32, 16, 8, 4, 2, 1 };
#define read_bit(b, n)      (((b) & pgm_read_byte(bit_mask + (n))) ? 1 : 0)

Adafruit_SSD1306<SCREEN_WIDTH, SCREEN_HEIGHT> display;

// FPS control
double delta = 1.0;
uint32_t lastFrameTime = 0;

#ifdef OPTIMIZE_SSD1306
uint8_t *display_buf = nullptr;
#endif

uint8_t zbuffer[ZBUFFER_SIZE];

void setupDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    while (1) {
      // Halts if display initialization fails
    }
  }

#ifdef OPTIMIZE_SSD1306
  display_buf = display.getBuffer();
#endif

  memset(zbuffer, 0xFF, ZBUFFER_SIZE);
}

void fps() {
  while (millis() - lastFrameTime < FRAME_TIME);
  delta = (double)(millis() - lastFrameTime) / FRAME_TIME;
  lastFrameTime = millis();
}

double getActualFps() {
  return 1000.0 / (FRAME_TIME * delta);
}

void drawByte(uint8_t x, uint8_t y, uint8_t b) {
#ifdef OPTIMIZE_SSD1306
  display_buf[(y / 8) * SCREEN_WIDTH + x] = b;
#else
  (void)x;
  (void)y;
  (void)b;
#endif
}

bool getGradientPixel(uint8_t x, uint8_t y, uint8_t i) {
  if (i == 0) return false;
  if (i >= GRADIENT_COUNT - 1) return true;

  uint8_t index = i * GRADIENT_WIDTH * GRADIENT_HEIGHT
                  + (y * GRADIENT_WIDTH) % (GRADIENT_WIDTH * GRADIENT_HEIGHT)
                  + (x / GRADIENT_HEIGHT) % GRADIENT_WIDTH;

  return read_bit(pgm_read_byte(gradient + index), x % 8) != 0;
}

void fadeScreen(uint8_t intensity, bool color) {
  for (uint8_t x = 0; x < SCREEN_WIDTH; x++) {
    for (uint8_t y = 0; y < SCREEN_HEIGHT; y++) {
      if (getGradientPixel(x, y, intensity)) {
        drawPixel(x, y, color, false);
      }
    }
  }
}

void drawPixel(int8_t x, int8_t y, bool color, bool raycasterViewport) {
  static_assert(SCREEN_WIDTH == 128, "drawPixel assumes int8_t x in 0..127");
  int8_t max_y = (int8_t)(raycasterViewport ? RENDER_HEIGHT : SCREEN_HEIGHT);
  if (x < 0 || y < 0 || y >= max_y) {
    return;
  }

#ifdef OPTIMIZE_SSD1306
  if (color) {
    display_buf[x + (y / 8) * SCREEN_WIDTH] |= (1 << (y & 7));
  } else {
    display_buf[x + (y / 8) * SCREEN_WIDTH] &= ~(1 << (y & 7));
  }
#else
  display.drawPixel(x, y, color);
#endif
}

void drawVLine(uint8_t x, int8_t start_y, int8_t end_y, uint8_t intensity) {
  int8_t y;
  int8_t lower_y = max(min(start_y, end_y), (int8_t)0);
  int8_t higher_y = min(max(start_y, end_y), (int8_t)(RENDER_HEIGHT - 1));
  uint8_t c;

  if (lower_y > higher_y) {
    return;
  }

#ifdef OPTIMIZE_SSD1306
  uint8_t bp = 7;
  uint8_t b;
  for (c = 0; c < RES_DIVIDER; c++) {
    y = lower_y;
    b = 0;
    bp = 7;
    while (y <= higher_y) {
      bp = (uint8_t)(y % 8);
      b = b | ((uint8_t)getGradientPixel(x + c, (uint8_t)y, intensity) << bp);

      if (bp == 7) {
        drawByte(x + c, (uint8_t)y, b);
        b = 0;
      }

      y++;
    }

    if (bp != 7) {
      drawByte(x + c, (uint8_t)(y - 1), b);
    }
  }
#else
  y = lower_y;
  while (y <= higher_y) {
    for (c = 0; c < RES_DIVIDER; c++) {
      if (getGradientPixel(x + c, (uint8_t)y, intensity)) {
        drawPixel((int8_t)(x + c), y, true, true);
      }
    }
    y++;
  }
#endif
}

void drawSprite(
  int8_t x, int8_t y,
  const uint8_t bitmap[], const uint8_t mask[],
  int16_t w, int16_t h,
  uint8_t sprite,
  double distance
) {
  uint8_t tw = (uint8_t)((double)w / distance);
  uint8_t th = (uint8_t)((double)h / distance);
  uint8_t byte_width = (uint8_t)(w / 8);
  uint8_t pixel_size = (uint8_t)max(1.0, 1.0 / distance);
  uint16_t sprite_offset = (uint16_t)byte_width * (uint16_t)h * sprite;

  bool pixel;
  bool maskPixel;

  // Z-buffer occlusion check (sera aprimorado em fase posterior)
  if (zbuffer[min(max((int16_t)x, (int16_t)0), (int16_t)(ZBUFFER_SIZE - 1)) / Z_RES_DIVIDER] < distance * DISTANCE_MULTIPLIER) {
    return;
  }

  for (uint8_t ty = 0; ty < th; ty += pixel_size) {
    if (y + ty < 0 || y + ty >= (int16_t)RENDER_HEIGHT) {
      continue;
    }

    uint8_t sy = (uint8_t)(ty * distance);

    for (uint8_t tx = 0; tx < tw; tx += pixel_size) {
      uint8_t sx = (uint8_t)(tx * distance);
      uint16_t byte_offset = sprite_offset + (uint16_t)sy * byte_width + sx / 8;

      if (x + tx < 0 || x + tx >= (int16_t)SCREEN_WIDTH) {
        continue;
      }

      maskPixel = read_bit(pgm_read_byte(mask + byte_offset), sx % 8) != 0;

      if (maskPixel) {
        pixel = read_bit(pgm_read_byte(bitmap + byte_offset), sx % 8) != 0;
        for (uint8_t ox = 0; ox < pixel_size; ox++) {
          for (uint8_t oy = 0; oy < pixel_size; oy++) {
            drawPixel((int8_t)(x + tx + ox), (int8_t)(y + ty + oy), pixel, true);
          }
        }
      }
    }
  }
}

void drawChar(int8_t x, int8_t y, char ch) {
  uint8_t c = 0;
  uint8_t n;
  uint8_t bOffset;
  uint8_t b;
  uint8_t line = 0;

  while (CHAR_MAP[c] != ch && CHAR_MAP[c] != '\0') c++;

  bOffset = c / 2;
  for (; line < CHAR_HEIGHT; line++) {
    b = pgm_read_byte(bmp_font + (line * bmp_font_width + bOffset));
    for (n = 0; n < CHAR_WIDTH; n++) {
      if (read_bit(b, (c % 2 == 0 ? 0 : 4) + n)) {
        drawPixel((int8_t)(x + n), (int8_t)(y + line), true, false);
      }
    }
  }
}

void drawText(int8_t x, int8_t y, const char *txt, uint8_t space) {
  uint8_t pos = (uint8_t)x;
  uint8_t i = 0;
  char ch;
  while ((ch = txt[i]) != '\0') {
    drawChar((int8_t)pos, y, ch);
    i++;
    pos += CHAR_WIDTH + space;

    if (pos > SCREEN_WIDTH) return;
  }
}

void drawText(int8_t x, int8_t y, const __FlashStringHelper *txt_p, uint8_t space) {
  uint8_t pos = (uint8_t)x;
  uint8_t i = 0;
  char ch;
  while (pos < SCREEN_WIDTH && (ch = (char)F_char(txt_p, i)) != '\0') {
    drawChar((int8_t)pos, y, ch);
    i++;
    pos += CHAR_WIDTH + space;
  }
}

void drawText(uint8_t x, uint8_t y, uint8_t num) {
  char buf[4];
  itoa(num, buf, 10);
  drawText((int8_t)x, (int8_t)y, buf);
}
