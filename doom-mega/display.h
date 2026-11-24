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

extern uint8_t zbuffer[ZBUFFER_SIZE];

void setupDisplay();
void fps();
double getActualFps();
bool getGradientPixel(uint8_t x, uint8_t y, uint8_t i);
void fadeScreen(uint8_t intensity, bool color = false);
void drawByte(uint8_t x, uint8_t y, uint8_t b);
void drawPixel(int8_t x, int8_t y, bool color, bool raycasterViewport = false);
void drawVLine(uint8_t x, int8_t start_y, int8_t end_y, uint8_t intensity);
void drawSprite(
  int8_t x, int8_t y,
  const uint8_t bitmap[], const uint8_t mask[],
  int16_t w, int16_t h,
  uint8_t sprite,
  double distance
);
void drawChar(int8_t x, int8_t y, char ch);
void drawText(int8_t x, int8_t y, const char *txt, uint8_t space = 1);
void drawText(int8_t x, int8_t y, const __FlashStringHelper *txt_p, uint8_t space = 1);
void drawText(uint8_t x, uint8_t y, uint8_t num);

#endif // DISPLAY_H
