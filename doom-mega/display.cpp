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

uint16_t zbuffer[SCREEN_WIDTH];

void clearZBuffer() {
  // Cada byte 0xFF preenche todos os elementos uint16_t com Z_CLEAR (0xFFFF)
  memset(zbuffer, 0xFF, sizeof(zbuffer));
}

uint16_t encodeZDistance(double distance) {
  if (distance <= 0.0) return 0;
  uint32_t scaled = (uint32_t)(distance * (double)Z_SCALE);
  return (scaled > Z_MAX) ? Z_MAX : (uint16_t)scaled;
}

void setupDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    while (1) {
      // Halts if display initialization fails
    }
  }

#ifdef OPTIMIZE_SSD1306
  display_buf = display.getBuffer();
#endif

  clearZBuffer();
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

void fadeScreen(uint8_t intensity, bool color, bool onlyViewport) {
  uint8_t start_y = onlyViewport ? RENDER_TOP : 0;
  uint8_t end_y   = onlyViewport ? RENDER_BOTTOM : SCREEN_HEIGHT;

  for (uint8_t x = 0; x < SCREEN_WIDTH; x++) {
    for (uint8_t y = start_y; y < end_y; y++) {
      if (getGradientPixel(x, y, intensity)) {
        drawPixel(x, y, color, onlyViewport);
      }
    }
  }
}

void drawPixel(int16_t x, int16_t y, bool color, bool raycasterViewport) {
  int16_t min_y = raycasterViewport ? (int16_t)RENDER_TOP : 0;
  int16_t max_y = raycasterViewport ? (int16_t)RENDER_BOTTOM : (int16_t)SCREEN_HEIGHT;
  if (x < 0 || x >= (int16_t)SCREEN_WIDTH || y < min_y || y >= max_y) {
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

void drawVLine(uint8_t x, int16_t start_y, int16_t end_y, uint8_t intensity) {
  int16_t lower_y  = max(min(start_y, end_y), (int16_t)RENDER_TOP);
  int16_t higher_y = min(max(start_y, end_y), (int16_t)(RENDER_BOTTOM - 1));

  if (lower_y > higher_y) {
    return;
  }

#ifdef OPTIMIZE_SSD1306
  static_assert((RENDER_TOP % 8) == 0, "RENDER_TOP must be page-aligned (multiple of 8)");
  static_assert((RENDER_BOTTOM % 8) == 0, "RENDER_BOTTOM must be page-aligned (multiple of 8)");

  for (uint8_t c = 0; c < RES_DIVIDER && (x + c) < SCREEN_WIDTH; c++) {
    int16_t y  = lower_y;
    uint8_t b  = 0;
    uint8_t bp = 7;

    while (y <= higher_y) {
      bp = (uint8_t)(y & 7);
      if (getGradientPixel(x + c, (uint8_t)y, intensity)) {
        b |= (uint8_t)(1 << bp);
      }

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
  for (int16_t y = lower_y; y <= higher_y; y++) {
    for (uint8_t c = 0; c < RES_DIVIDER && (x + c) < SCREEN_WIDTH; c++) {
      if (getGradientPixel(x + c, (uint8_t)y, intensity)) {
        drawPixel(x + c, y, true, true);
      }
    }
  }
#endif
}

void drawExitVLine(uint8_t x, int16_t start_y, int16_t end_y) {
  int16_t lower_y  = max(min(start_y, end_y), (int16_t)RENDER_TOP);
  int16_t higher_y = min(max(start_y, end_y), (int16_t)(RENDER_BOTTOM - 1));

  if (lower_y > higher_y) {
    return;
  }

  // Padrao zebrado animado (listras diagonais em alto contraste) para identificar E_EXIT no 3D
  uint8_t phase = (uint8_t)((millis() / 120) & 3);
  for (int16_t y = lower_y; y <= higher_y; y++) {
    bool is_border = (y == lower_y || y == higher_y);
    for (uint8_t c = 0; c < RES_DIVIDER && (x + c) < SCREEN_WIDTH; c++) {
      bool stripe = ((((uint8_t)(x + c) + (uint8_t)y + phase) & 3) < 2);
      if (is_border || stripe) {
        drawPixel(x + c, y, true, true);
      }
    }
  }
}

void drawBitmapViewport(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, bool color) {
  int16_t byteWidth = (w + 7) / 8;

  for (int16_t j = 0; j < h; j++) {
    int16_t py = y + j;
    if (py < (int16_t)RENDER_TOP || py >= (int16_t)RENDER_BOTTOM) {
      continue;
    }

    uint8_t byte = 0;
    for (int16_t i = 0; i < w; i++) {
      if (i & 7) {
        byte <<= 1;
      } else {
        byte = pgm_read_byte(&bitmap[j * byteWidth + (i >> 3)]);
      }

      if (byte & 0x80) {
        drawPixel(x + i, py, color, true);
      }
    }
  }
}

void drawSprite(
  int16_t x, int16_t y,
  const uint8_t bitmap[], const uint8_t mask[],
  int16_t w, int16_t h,
  uint8_t sprite,
  double distance
) {
  if (distance <= 0.05 || w <= 0 || h <= 0) {
    return;
  }

  int16_t tw = (int16_t)((double)w / distance);
  int16_t th = (int16_t)((double)h / distance);
  if (tw <= 0 || th <= 0) {
    return;
  }

  // Clipping horizontal e vertical estrito ao viewport azul [RENDER_TOP, RENDER_BOTTOM)
  int16_t tx_start = (x < 0) ? -x : 0;
  int16_t tx_end   = (x + tw > (int16_t)SCREEN_WIDTH) ? ((int16_t)SCREEN_WIDTH - x) : tw;
  if (tx_start >= tx_end) {
    return;
  }

  int16_t ty_start = (y < (int16_t)RENDER_TOP) ? ((int16_t)RENDER_TOP - y) : 0;
  int16_t ty_end   = (y + th > (int16_t)RENDER_BOTTOM) ? ((int16_t)RENDER_BOTTOM - y) : th;
  if (ty_start >= ty_end) {
    return;
  }

  uint8_t  byte_width    = (uint8_t)(w / 8);
  uint16_t sprite_offset = (uint16_t)byte_width * (uint16_t)h * sprite;
  uint16_t sprite_z      = encodeZDistance(distance);

  // Passos de textura em ponto fixo 8.8 (evita divisoes dentro dos loops de pixel)
  uint16_t step_x      = (uint16_t)(((uint32_t)w << 8) / (uint16_t)tw);
  uint16_t step_y      = (uint16_t)(((uint32_t)h << 8) / (uint16_t)th);
  uint16_t tex_x       = (uint16_t)((uint32_t)tx_start * step_x);
  uint16_t tex_y_init  = (uint16_t)((uint32_t)ty_start * step_y);

  for (int16_t tx = tx_start; tx < tx_end; tx++, tex_x += step_x) {
    int16_t screen_x = x + tx;

    // Teste de oclusao por coluna de tela contra o Z-buffer de 16 bits
    if (sprite_z >= zbuffer[screen_x]) {
      continue;
    }

    uint8_t sx = (uint8_t)(tex_x >> 8);
    if (sx >= (uint8_t)w) sx = (uint8_t)(w - 1);
    uint8_t sx_byte   = sx >> 3;
    uint8_t bit_index = sx & 7;

    uint16_t tex_y = tex_y_init;
    for (int16_t ty = ty_start; ty < ty_end; ty++, tex_y += step_y) {
      uint8_t sy = (uint8_t)(tex_y >> 8);
      if (sy >= (uint8_t)h) sy = (uint8_t)(h - 1);

      uint16_t byte_offset = sprite_offset + (uint16_t)sy * byte_width + sx_byte;
      if (read_bit(pgm_read_byte(mask + byte_offset), bit_index)) {
        bool pixel = read_bit(pgm_read_byte(bitmap + byte_offset), bit_index) != 0;
        drawPixel(screen_x, y + ty, pixel, true);
      }
    }
  }
}

void drawChar(int16_t x, int16_t y, char ch) {
  uint8_t c = 0;
  while (CHAR_MAP[c] != ch && CHAR_MAP[c] != '\0') c++;
  if (CHAR_MAP[c] == '\0') return; // Ignora caracteres fora da tabela da fonte

  uint8_t bOffset = c / 2;
  for (uint8_t line = 0; line < CHAR_HEIGHT; line++) {
    uint8_t b = pgm_read_byte(bmp_font + (line * bmp_font_width + bOffset));
    for (uint8_t n = 0; n < CHAR_WIDTH; n++) {
      if (read_bit(b, (c % 2 == 0 ? 0 : 4) + n)) {
        drawPixel(x + n, y + line, true, false);
      }
    }
  }
}

void drawText(int16_t x, int16_t y, const char *txt, uint8_t space) {
  int16_t pos = x;
  uint8_t i = 0;
  char ch;
  while ((ch = txt[i]) != '\0') {
    drawChar(pos, y, ch);
    i++;
    pos += (int16_t)(CHAR_WIDTH + space);

    if (pos >= (int16_t)SCREEN_WIDTH) return;
  }
}

void drawText(int16_t x, int16_t y, const __FlashStringHelper *txt_p, uint8_t space) {
  int16_t pos = x;
  uint8_t i = 0;
  char ch;
  while (pos < (int16_t)SCREEN_WIDTH && (ch = (char)F_char(txt_p, i)) != '\0') {
    drawChar(pos, y, ch);
    i++;
    pos += (int16_t)(CHAR_WIDTH + space);
  }
}

void drawText(uint8_t x, uint8_t y, uint8_t num) {
  char buf[4];
  itoa(num, buf, 10);
  drawText((int16_t)x, (int16_t)y, buf);
}

void drawTextNum(int16_t x, int16_t y, uint16_t num) {
  char buf[6];
  utoa(num, buf, 10);
  drawText(x, y, buf);
}
