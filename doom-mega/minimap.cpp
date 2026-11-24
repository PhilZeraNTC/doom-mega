#include <Arduino.h>
#include "minimap.h"
#include "constants.h"
#include "display.h"
#include "level.h"
#include "types.h"

namespace {

constexpr int8_t HALF_CELLS = MINIMAP_CELLS / 2; // 10 -> janela [-10 .. +9]

void drawBlock2x2(int16_t x, int16_t y) {
  drawPixel(x,     y,     true, true);
  drawPixel(x + 1, y,     true, true);
  drawPixel(x,     y + 1, true, true);
  drawPixel(x + 1, y + 1, true, true);
}

void drawBorder() {
  uint8_t x1 = MINIMAP_X + MINIMAP_SIZE - 1;
  uint8_t y1 = MINIMAP_Y + MINIMAP_SIZE - 1;

  for (uint8_t x = MINIMAP_X; x <= x1; x++) {
    drawPixel(x, MINIMAP_Y, true, true);
    drawPixel(x, y1,        true, true);
  }
  for (uint8_t y = MINIMAP_Y + 1; y < y1; y++) {
    drawPixel(MINIMAP_X, y, true, true);
    drawPixel(x1,        y, true, true);
  }
}

} // namespace

void minimap_render(
  const uint8_t level[],
  const Player &player,
  const Entity entities[],
  uint8_t num_entities
) {
  // Fundo escuro sobre o 3D para maxima legibilidade
  display.clearRect(MINIMAP_X + 1, MINIMAP_Y + 1, MINIMAP_SIZE - 2, MINIMAP_SIZE - 2);
  drawBorder();

  int16_t player_cx = (int16_t)player.pos.x;
  int16_t player_cy = (int16_t)player.pos.y;

  uint32_t now_ms     = millis();
  bool     exit_blink = ((now_ms / 180UL) & 1) == 0;
  bool     enemy_blink = ((now_ms / 150UL) & 1) == 0;

  // 1) Paredes (E_WALL) e Saida (E_EXIT) centradas no jogador (Norte = +Y para cima)
  for (int8_t dy = -HALF_CELLS; dy < HALF_CELLS; dy++) {
    int16_t my = player_cy + dy;
    if (my < 0 || my >= LEVEL_HEIGHT) {
      continue;
    }

    int16_t sy = (int16_t)MINIMAP_INNER_Y + (int16_t)((HALF_CELLS - 1) - dy) * MINIMAP_SCALE;

    for (int8_t dx = -HALF_CELLS; dx < HALF_CELLS; dx++) {
      int16_t mx = player_cx + dx;
      if (mx < 0 || mx >= LEVEL_WIDTH) {
        continue;
      }

      uint8_t block = level_get_block(level, (uint8_t)mx, (uint8_t)my);
      if (block == E_WALL) {
        int16_t sx = (int16_t)MINIMAP_INNER_X + (int16_t)(dx + HALF_CELLS) * MINIMAP_SCALE;
        drawBlock2x2(sx, sy);
      } else if (block == E_EXIT && exit_blink) {
        int16_t sx = (int16_t)MINIMAP_INNER_X + (int16_t)(dx + HALF_CELLS) * MINIMAP_SCALE;
        // Destaque piscante em padrao de cruz/bloco para a saida
        drawBlock2x2(sx, sy);
      }
    }
  }

  // 2) Entidades ativas dentro da janela 20x20
  for (uint8_t i = 0; i < num_entities; i++) {
    if (entities[i].state == S_HIDDEN || entities[i].state == S_DEAD) {
      continue;
    }

    int16_t rel_x = (int16_t)entities[i].pos.x - player_cx;
    int16_t rel_y = (int16_t)entities[i].pos.y - player_cy;
    if (rel_x < -HALF_CELLS || rel_x >= HALF_CELLS || rel_y < -HALF_CELLS || rel_y >= HALF_CELLS) {
      continue;
    }

    int16_t sx = (int16_t)MINIMAP_INNER_X + (rel_x + HALF_CELLS) * MINIMAP_SCALE;
    int16_t sy = (int16_t)MINIMAP_INNER_Y + ((HALF_CELLS - 1) - rel_y) * MINIMAP_SCALE;
    uint8_t type = uid_get_type(entities[i].uid);

    if (type == E_ENEMY) {
      // Inimigos vivos piscam em diagonal para diferenciar de paredes 2x2
      if (enemy_blink) {
        drawPixel(sx,     sy,     true, true);
        drawPixel(sx + 1, sy + 1, true, true);
      }
    } else if (type == E_MEDIKIT || type == E_KEY) {
      // Itens coletaveis como ponto discreto 1x1
      drawPixel(sx, sy, true, true);
    }
  }

  // 3) Jogador no centro do radar (celula 0,0) + vetor de direcao (player.dir)
  int16_t pcx = (int16_t)MINIMAP_INNER_X + HALF_CELLS * MINIMAP_SCALE;       // 104
  int16_t pcy = (int16_t)MINIMAP_INNER_Y + (HALF_CELLS - 1) * MINIMAP_SCALE; // 38
  drawBlock2x2(pcx, pcy);

  // Linha direcional de 3 passos apontando no sentido de player.dir (eixo Y invertido na tela)
  double origin_x = (double)pcx + 0.5;
  double origin_y = (double)pcy + 0.5;
  for (uint8_t step = 2; step <= 4; step++) {
    int16_t vx = (int16_t)(origin_x + player.dir.x * (double)step);
    int16_t vy = (int16_t)(origin_y - player.dir.y * (double)step);
    if (vx >= (int16_t)MINIMAP_INNER_X && vx < (int16_t)(MINIMAP_INNER_X + MINIMAP_CELLS * MINIMAP_SCALE) &&
        vy >= (int16_t)MINIMAP_INNER_Y && vy < (int16_t)(MINIMAP_INNER_Y + MINIMAP_CELLS * MINIMAP_SCALE)) {
      drawPixel(vx, vy, true, true);
    }
  }
}
