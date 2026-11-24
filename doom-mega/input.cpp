#include <Arduino.h>
#include <stdint.h>
#include "input.h"
#include "pinout.h"

namespace {

struct DebouncedButton {
  uint8_t  pin;
  bool     raw_state;
  bool     stable_state;
  bool     just_pressed;
  uint32_t last_change_ms;
};

// Centros calibrados em repouso no boot (escala ADC 10 bits: 0..1023)
int16_t center_l_x = 512;
int16_t center_l_y = 512;
int16_t center_r_x = 512;
int16_t center_r_y = 512;

// Eixos normalizados [-1.0 .. +1.0] atualizados por input_update()
float axis_move_forward = 0.0f;
float axis_move_strafe  = 0.0f;
float axis_turn         = 0.0f;
float axis_right_y      = 0.0f;

// Estado com debounce dos 3 botoes digitais (INPUT_PULLUP -> ativo em LOW)
DebouncedButton btn_fire;
DebouncedButton btn_joy_l_sw;
DebouncedButton btn_joy_r_sw;

void initButton(DebouncedButton &btn, uint8_t pin) {
  pinMode(pin, INPUT_PULLUP);
  bool pressed = (digitalRead(pin) == LOW);
  btn.pin            = pin;
  btn.raw_state      = pressed;
  btn.stable_state   = pressed;
  btn.just_pressed   = false;
  btn.last_change_ms = millis();
}

void updateButton(DebouncedButton &btn, uint32_t now) {
  btn.just_pressed = false;
  bool reading = (digitalRead(btn.pin) == LOW);

  if (reading != btn.raw_state) {
    btn.raw_state      = reading;
    btn.last_change_ms = now;
  }

  if ((now - btn.last_change_ms) >= BTN_DEBOUNCE_MS) {
    if (reading != btn.stable_state) {
      if (reading && !btn.stable_state) {
        btn.just_pressed = true; // Borda de descida (solto -> pressionado)
      }
      btn.stable_state = reading;
    }
  }
}

int16_t readAverageAxis(uint8_t pin) {
  // Primeira leitura descartada para estabilizar o capacitor S&H apos troca de canal MUX
  (void)analogRead(pin);
  int32_t sum = 0;
  for (uint8_t i = 0; i < JOY_CAL_SAMPLES; i++) {
    sum += analogRead(pin);
  }
  return (int16_t)(sum / JOY_CAL_SAMPLES);
}

// Aplica offset de centro + deadzone com remapeamento continuo para [-1.0 .. +1.0]
float normalizeAxis(int16_t raw, int16_t center, bool invert) {
  int16_t diff = raw - center;
  float norm = 0.0f;

  if (diff > JOY_DEADZONE) {
    int16_t active = diff - JOY_DEADZONE;
    int16_t span   = (1023 - center) - JOY_DEADZONE;
    if (span < 1) span = 1;
    norm = (float)active / (float)span;
    if (norm > 1.0f) norm = 1.0f;
  } else if (diff < -JOY_DEADZONE) {
    int16_t active = (-diff) - JOY_DEADZONE;
    int16_t span   = center - JOY_DEADZONE;
    if (span < 1) span = 1;
    norm = -((float)active / (float)span);
    if (norm < -1.0f) norm = -1.0f;
  } else {
    return 0.0f;
  }

  return invert ? -norm : norm;
}

} // namespace

void input_calibrate() {
  center_l_x = readAverageAxis(PIN_JOY_L_X);
  center_l_y = readAverageAxis(PIN_JOY_L_Y);
  center_r_x = readAverageAxis(PIN_JOY_R_X);
  center_r_y = readAverageAxis(PIN_JOY_R_Y);
}

void input_setup() {
  initButton(btn_joy_l_sw, PIN_JOY_L_SW);
  initButton(btn_joy_r_sw, PIN_JOY_R_SW);
  initButton(btn_fire,     PIN_BTN_FIRE);

  input_calibrate();
  input_update();
}

void input_update() {
  uint32_t now = millis();

  // Leitura dos 4 canais ADC com remapeamento continuo apos deadzone
  int16_t raw_l_x = (int16_t)analogRead(PIN_JOY_L_X);
  int16_t raw_l_y = (int16_t)analogRead(PIN_JOY_L_Y);
  int16_t raw_r_x = (int16_t)analogRead(PIN_JOY_R_X);
  int16_t raw_r_y = (int16_t)analogRead(PIN_JOY_R_Y);

  axis_move_strafe  = normalizeAxis(raw_l_x, center_l_x, JOY_L_INVERT_X);
  axis_move_forward = normalizeAxis(raw_l_y, center_l_y, JOY_L_INVERT_Y);
  axis_turn         = normalizeAxis(raw_r_x, center_r_x, JOY_R_INVERT_X);
  axis_right_y      = normalizeAxis(raw_r_y, center_r_y, JOY_R_INVERT_Y);

  // Debounce e deteccao de borda dos 3 botoes digitais
  updateButton(btn_fire,     now);
  updateButton(btn_joy_l_sw, now);
  updateButton(btn_joy_r_sw, now);
}

float input_move_forward() {
  return axis_move_forward;
}

float input_move_strafe() {
  return axis_move_strafe;
}

float input_turn() {
  return axis_turn;
}

float input_right_y() {
  return axis_right_y;
}

bool input_sprint() {
  return btn_joy_l_sw.stable_state;
}

bool input_fire() {
  return btn_fire.stable_state;
}

bool input_fire_just_pressed() {
  return btn_fire.just_pressed;
}

bool input_action_sw_right_just_pressed() {
  return btn_joy_r_sw.just_pressed;
}
