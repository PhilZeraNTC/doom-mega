#include <stdint.h>
#include <math.h>
#include "types.h"
#include "constants.h"

template <class T>
static inline T square_val(T value) {
  return value * value;
}

Coords create_coords(double x, double y) {
  return { x, y };
}

uint8_t coords_distance(Coords *a, Coords *b) {
  return (uint8_t)(sqrt(square_val(a->x - b->x) + square_val(a->y - b->y)) * DISTANCE_MULTIPLIER);
}

UID create_uid(EType type, uint8_t x, uint8_t y) {
  return (UID)(((((uint16_t)y << LEVEL_WIDTH_BASE) | (uint16_t)x) << 4) | (uint16_t)(type & 0x0F));
}

EType uid_get_type(UID uid) {
  return (EType)(uid & 0x0F);
}

uint8_t uid_get_x(UID uid) {
  return (uint8_t)((uid >> 4) & (LEVEL_WIDTH - 1));
}

uint8_t uid_get_y(UID uid) {
  return (uint8_t)(uid >> (4 + LEVEL_WIDTH_BASE));
}
