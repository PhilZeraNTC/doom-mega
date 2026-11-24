#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <stdint.h>
#include "constants.h"
#include "SSD1306.h"

extern Adafruit_SSD1306<SCREEN_WIDTH, SCREEN_HEIGHT> display;

extern double delta;
extern uint32_t lastFrameTime;

#ifdef OPTIMIZE_SSD1306
extern uint8_t *display_buf;
#endif

// Z-buffer de 16 bits por coluna de tela (128 * 2 = 256 bytes de SRAM)
extern uint16_t zbuffer[SCREEN_WIDTH];

void setupDisplay();
void clearZBuffer();
uint16_t encodeZDistance(double distance);
void fps();
double getActualFps();
bool getGradientPixel(uint8_t x, uint8_t y, uint8_t i);
void fadeScreen(uint8_t intensity, bool color = false, bool onlyViewport = false);
void drawByte(uint8_t x, uint8_t y, uint8_t b);
void drawPixel(int16_t x, int16_t y, bool color, bool raycasterViewport = false);
void drawVLine(uint8_t x, int16_t start_y, int16_t end_y, uint8_t intensity);
void drawExitVLine(uint8_t x, int16_t start_y, int16_t end_y);
void drawBitmapViewport(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, bool color);
void drawSprite(
  int16_t x, int16_t y,
  const uint8_t bitmap[], const uint8_t mask[],
  int16_t w, int16_t h,
  uint8_t sprite,
  double distance
);
void drawChar(int16_t x, int16_t y, char ch);
void drawText(int16_t x, int16_t y, const char *txt, uint8_t space = 1);
void drawText(int16_t x, int16_t y, const __FlashStringHelper *txt_p, uint8_t space = 1);
void drawText(uint8_t x, uint8_t y, uint8_t num);
void drawTextNum(int16_t x, int16_t y, uint16_t num);

#endif // DISPLAY_H
