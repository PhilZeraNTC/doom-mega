#include <Arduino.h>
#include <string.h>
#include "hud.h"
#include "constants.h"
#include "display.h"

namespace {

// Layout da faixa amarela superior (128 x 16 px -> y = 0..15):
//   Esquerda (x = 1..52):
//     - Icone de vida "{}"      : (1, 1)   -> x = 1..8,   y = 1..6
//     - Valor numerico de vida  : (11, 1)  -> x = 11..24, y = 1..6
//     - Barra visual de vida    : moldura (1, 9, 52x5), interior preenchivel (2..51, 10..12 = 50 px)
//   Centro-Esquerda (x = 60..81):
//     - Icone de chave "[]"     : (60, 1)  -> x = 60..67, y = 1..6
//     - Valor numerico chaves   : (70, 1)  -> x = 70..78, y = 1..6
//     - Indicador SPRINT "RUN"  : (60, 8)  -> x = 60..73, y = 8..13
//   Centro-Direita (x = 88..104):
//     - Indicador RADAR "MAP"   : (89, 1)  -> x = 89..102, y = 1..6
//   Direita (x = 111..126):
//     - Rotulo "FPS"            : (112, 1) -> x = 112..125, y = 1..6
//     - Valor FPS (dir-alinhado): fim em x = 125, y = 8..13
//   Separador horizontal        : y = 15 (x = 0..127)

constexpr uint8_t HP_ICON_X       = 1;
constexpr uint8_t HP_ICON_Y       = 1;
constexpr uint8_t HP_VAL_X        = 11;
constexpr uint8_t HP_VAL_Y        = 1;
constexpr uint8_t HP_VAL_CLEAR_W  = 16;

constexpr uint8_t HP_BAR_X        = 1;
constexpr uint8_t HP_BAR_Y        = 9;
constexpr uint8_t HP_BAR_W        = 52; // 2 px de borda + 50 px internos
constexpr uint8_t HP_BAR_H        = 5;  // 2 px de borda + 3 px internos
constexpr uint8_t HP_BAR_INNER_W  = 50;

constexpr uint8_t KEY_ICON_X      = 60;
constexpr uint8_t KEY_ICON_Y      = 1;
constexpr uint8_t KEY_VAL_X       = 70;
constexpr uint8_t KEY_VAL_Y       = 1;
constexpr uint8_t KEY_VAL_CLEAR_W = 12;

constexpr uint8_t SPRINT_X        = 60;
constexpr uint8_t SPRINT_Y        = 8;
constexpr uint8_t SPRINT_CLEAR_W  = 16;

constexpr uint8_t MAP_IND_X       = 89;
constexpr uint8_t MAP_IND_Y       = 1;
constexpr uint8_t MAP_IND_CLEAR_W = 16;

constexpr uint8_t FPS_LBL_X       = 112;
constexpr uint8_t FPS_LBL_Y       = 1;
constexpr uint8_t FPS_VAL_Y       = 8;
constexpr uint8_t FPS_CLEAR_X     = 111;
constexpr uint8_t FPS_CLEAR_W     = 16;

constexpr uint8_t HUD_DIVIDER_Y   = 15;

// Caches para evitar redesenho redundante a cada frame
uint8_t last_fps_drawn     = 255;
bool    last_sprint_drawn  = false;
bool    last_minimap_drawn = false;
bool    stats_initialized  = false;

void drawHLine(uint8_t x0, uint8_t x1, uint8_t y, bool color) {
  for (uint8_t x = x0; x <= x1; x++) {
    drawPixel(x, y, color, false);
  }
}

void drawHealthBarFrame() {
  uint8_t x1 = HP_BAR_X + HP_BAR_W - 1;
  uint8_t y1 = HP_BAR_Y + HP_BAR_H - 1;

  drawHLine(HP_BAR_X, x1, HP_BAR_Y, true);
  drawHLine(HP_BAR_X, x1, y1, true);
  for (uint8_t y = HP_BAR_Y + 1; y < y1; y++) {
    drawPixel(HP_BAR_X, y, true, false);
    drawPixel(x1, y, true, false);
  }
}

void updateHealthBarFill(uint8_t health) {
  uint8_t clamped = (health > 100) ? 100 : health;
  uint8_t fill_w  = (uint8_t)(((uint16_t)clamped * HP_BAR_INNER_W) / 100U);

  uint8_t inner_x0 = HP_BAR_X + 1;
  uint8_t inner_y0 = HP_BAR_Y + 1;
  uint8_t inner_y1 = HP_BAR_Y + HP_BAR_H - 2;

  for (uint8_t dx = 0; dx < HP_BAR_INNER_W; dx++) {
    bool on = (dx < fill_w);
    for (uint8_t y = inner_y0; y <= inner_y1; y++) {
      drawPixel(inner_x0 + dx, y, on, false);
    }
  }
}

} // namespace

void hud_render_full(uint8_t health, uint8_t keys, uint8_t fps_val, bool sprinting, bool minimap_on) {
#ifdef OPTIMIZE_SSD1306
  memset(display_buf, 0, SCREEN_WIDTH * (HUD_HEIGHT / 8));
#else
  display.clearRect(0, 0, SCREEN_WIDTH, HUD_HEIGHT);
#endif

  // Elementos estaticos
  drawText(HP_ICON_X, HP_ICON_Y, F("{}"), 0);
  drawHealthBarFrame();
  drawText(KEY_ICON_X, KEY_ICON_Y, F("[]"), 0);
  drawText(FPS_LBL_X, FPS_LBL_Y, F("FPS"), 1);

  // Linha divisoria limpa na base da faixa amarela (y = 15)
  drawHLine(0, SCREEN_WIDTH - 1, HUD_DIVIDER_Y, true);

  stats_initialized = false;
  hud_update_vitals(health, keys);
  hud_update_stats(fps_val, sprinting, minimap_on);
}

void hud_update_vitals(uint8_t health, uint8_t keys) {
  display.clearRect(HP_VAL_X, HP_VAL_Y, HP_VAL_CLEAR_W, 6);
  drawText(HP_VAL_X, HP_VAL_Y, health);
  updateHealthBarFill(health);

  display.clearRect(KEY_VAL_X, KEY_VAL_Y, KEY_VAL_CLEAR_W, 6);
  drawText(KEY_VAL_X, KEY_VAL_Y, keys);
}

void hud_update_stats(uint8_t fps_val, bool sprinting, bool minimap_on) {
  if (!stats_initialized || sprinting != last_sprint_drawn) {
    display.clearRect(SPRINT_X, SPRINT_Y, SPRINT_CLEAR_W, 6);
    if (sprinting) {
      drawText(SPRINT_X, SPRINT_Y, F("RUN"), 1);
    }
    last_sprint_drawn = sprinting;
  }

  if (!stats_initialized || minimap_on != last_minimap_drawn) {
    display.clearRect(MAP_IND_X, MAP_IND_Y, MAP_IND_CLEAR_W, 6);
    if (minimap_on) {
      drawText(MAP_IND_X, MAP_IND_Y, F("MAP"), 1);
    }
    last_minimap_drawn = minimap_on;
  }

  if (!stats_initialized || fps_val != last_fps_drawn) {
    display.clearRect(FPS_CLEAR_X, FPS_VAL_Y, FPS_CLEAR_W, 6);
    uint8_t fps_x;
    if (fps_val >= 100) {
      fps_x = 112; // 3 digitos (14 px -> x = 112..125)
    } else if (fps_val >= 10) {
      fps_x = 117; // 2 digitos (9 px  -> x = 117..125)
    } else {
      fps_x = 122; // 1 digito  (4 px  -> x = 122..125)
    }
    drawText(fps_x, FPS_VAL_Y, fps_val);
    last_fps_drawn = fps_val;
  }

  stats_initialized = true;
}
