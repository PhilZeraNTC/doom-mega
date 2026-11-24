#ifndef INPUT_H
#define INPUT_H

// Driver de entrada provisorio (Fase 2).
// Sera substituido pelo driver analogico dos joysticks KY-023 na Fase 3.

void input_setup();
bool input_up();
bool input_down();
bool input_left();
bool input_right();
bool input_fire();

#endif // INPUT_H
