#include <Arduino.h>
#include "input.h"
#include "pinout.h"

// Driver digital provisorio (Fase 2) utilizando INPUT_PULLUP (ativo em LOW).
// K_FIRE ja esta mapeado para PIN_BTN_FIRE (D26).
// Na Fase 3 este modulo sera substituido pela leitura analogica dos 2 joysticks KY-023.

void input_setup() {
  pinMode(K_LEFT, INPUT_PULLUP);
  pinMode(K_RIGHT, INPUT_PULLUP);
  pinMode(K_UP, INPUT_PULLUP);
  pinMode(K_DOWN, INPUT_PULLUP);
  pinMode(K_FIRE, INPUT_PULLUP);
}

bool input_left() {
  return digitalRead(K_LEFT) == LOW;
}

bool input_right() {
  return digitalRead(K_RIGHT) == LOW;
}

bool input_up() {
  return digitalRead(K_UP) == LOW;
}

bool input_down() {
  return digitalRead(K_DOWN) == LOW;
}

bool input_fire() {
  return digitalRead(K_FIRE) == LOW;
}
