#ifndef INPUT_H
#define INPUT_H

// Driver de entrada Twin-Stick FPS (2x KY-023 + Push-Button de tiro)
// - Calibracao dinamica do ponto neutro no boot (media de JOY_CAL_SAMPLES amostras)
// - Deadzone com remapeamento continuo [0.0 .. +-1.0]
// - Debounce nao-bloqueante via millis() e deteccao de borda nos botoes digitais

void  input_setup();
void  input_calibrate();
void  input_update();

float input_move_forward();               // -1.0 (tras) .. +1.0 (frente) — Joy L Y
float input_move_strafe();                // -1.0 (esquerda) .. +1.0 (direita) — Joy L X
float input_turn();                       // -1.0 (esquerda) .. +1.0 (direita) — Joy R X
float input_right_y();                    // -1.0 .. +1.0 — Joy R Y (disponivel para uso futuro)

bool  input_sprint();                     // PIN_JOY_L_SW pressionado (continuo)
bool  input_fire();                       // PIN_BTN_FIRE pressionado (continuo)
bool  input_fire_just_pressed();          // PIN_BTN_FIRE borda de descida (clique unico)
bool  input_action_sw_right_just_pressed(); // PIN_JOY_R_SW borda de descida (clique unico)

#endif // INPUT_H
