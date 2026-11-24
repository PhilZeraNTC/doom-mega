#ifndef PINOUT_H
#define PINOUT_H

#include <Arduino.h>

#if !defined(__AVR_ATmega2560__)
#warning "doom-mega pinout.h foi projetado para Arduino Mega 2560 (ATmega2560)."
#endif

// ============================================================================
// Contrato de Pinagem — Arduino Mega 2560 (ATmega2560)
// ============================================================================

// Display OLED 0.96" I2C SSD1306 128x64 (Dual-Color: 16 px amarelo + 48 px azul)
// Barramento TWI de hardware do ATmega2560 (fixo em hardware):
//   SDA = D20 (PD1)
//   SCL = D21 (PD0)
// Endereco I2C de 7 bits (TWI_ADDR = OLED_I2C_ADDR << 1 = 0x78 por padrao)
#ifndef OLED_I2C_ADDR
#define OLED_I2C_ADDR 0x3C
#endif

// Joystick Esquerdo — Movimento (Frente/Tras + Strafe + Sprint no SW)
constexpr uint8_t PIN_JOY_L_X  = A0;
constexpr uint8_t PIN_JOY_L_Y  = A1;
constexpr uint8_t PIN_JOY_L_SW = 22;

// Joystick Direito — Camera (Rotacao + Acao auxiliar no SW)
constexpr uint8_t PIN_JOY_R_X  = A2;
constexpr uint8_t PIN_JOY_R_Y  = A3;
constexpr uint8_t PIN_JOY_R_SW = 24;

// Botao Push-Button Comum — Tiro
constexpr uint8_t PIN_BTN_FIRE = 26;

// Buzzer Passivo — Timer1 OC1A (PB5 no ATmega2560 = Pino Digital 11)
// Nota: no ATmega328P (Uno/Nano) OC1A era D9; no Mega 2560 OC1A e D11.
constexpr uint8_t PIN_BUZZER   = 11;

// ============================================================================
// Calibracao e Ajuste Pos-Montagem — Controles (2x KY-023 + Push-Button)
// ============================================================================

// Zona morta ao redor do ponto neutro calibrado no boot (escala ADC 0..1023)
constexpr int16_t  JOY_DEADZONE    = 80;

// Quantidade de amostras ADC para media do centro em repouso no boot
constexpr uint8_t  JOY_CAL_SAMPLES = 16;

// Flags de inversao de eixo — ajuste para true/false conforme a orientacao
// fisica de cada modulo KY-023 na protoboard:
//   - JOY_L_INVERT_X: falso -> ADC crescente = strafe para a DIREITA (+1.0)
//   - JOY_L_INVERT_Y: true  -> ADC decrescente (empurrar p/ cima) = FRENTE (+1.0)
//   - JOY_R_INVERT_X: falso -> ADC crescente = girar para a DIREITA (+1.0)
//   - JOY_R_INVERT_Y: true  -> ADC decrescente (empurrar p/ cima) = +1.0
constexpr bool     JOY_L_INVERT_X  = false;
constexpr bool     JOY_L_INVERT_Y  = true;
constexpr bool     JOY_R_INVERT_X  = false;
constexpr bool     JOY_R_INVERT_Y  = true;

// Tempo de debounce por software para os 3 botoes digitais (ms)
constexpr uint16_t BTN_DEBOUNCE_MS = 30;

#endif // PINOUT_H
