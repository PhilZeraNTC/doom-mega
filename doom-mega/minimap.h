#ifndef MINIMAP_H
#define MINIMAP_H

#include <stdint.h>
#include "entities.h"

// Radar 2D em tempo real no canto direito da faixa azul (y = 16..63):
//   Moldura externa (1 px): x = 82..125, y = 18..61 (44 x 44 px)
//   Interior limpo        : x = 83..124, y = 19..60 (42 x 42 px)
//   Grade de 20x20 celulas: x = 84..123, y = 20..59 (40 x 40 px, escala 2x2 px/bloco)
constexpr uint8_t MINIMAP_X       = 82;
constexpr uint8_t MINIMAP_Y       = 18;
constexpr uint8_t MINIMAP_SIZE    = 44;
constexpr uint8_t MINIMAP_INNER_X = MINIMAP_X + 2; // 84
constexpr uint8_t MINIMAP_INNER_Y = MINIMAP_Y + 2; // 20
constexpr uint8_t MINIMAP_CELLS   = 20;
constexpr uint8_t MINIMAP_SCALE   = 2;

void minimap_render(
  const uint8_t level[],
  const Player &player,
  const Entity entities[],
  uint8_t num_entities
);

#endif // MINIMAP_H
