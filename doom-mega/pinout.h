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

// Joystick Esquerdo (Movimento: Frente/Tras + Strafe)
constexpr uint8_t PIN_JOY_L_X  = A0;
constexpr uint8_t PIN_JOY_L_Y  = A1;
constexpr uint8_t PIN_JOY_L_SW = 22;

// Joystick Direito (Camera: Rotacao)
constexpr uint8_t PIN_JOY_R_X  = A2;
constexpr uint8_t PIN_JOY_R_Y  = A3;
constexpr uint8_t PIN_JOY_R_SW = 24;

// Botao Push-Button Comum (Tiro)
constexpr uint8_t PIN_BTN_FIRE = 26;

// Buzzer Passivo — Timer1 OC1A (PB5 no ATmega2560 = Pino Digital 11)
// Nota: no ATmega328P (Uno/Nano) OC1A era D9; no Mega 2560 OC1A e D11.
constexpr uint8_t PIN_BUZZER   = 11;

// ============================================================================
// Mapeamento provisorio (Fase 2) — sera substituido pelo driver analogico
// dos joysticks KY-023 em input.cpp na Fase 3.
// ============================================================================
constexpr uint8_t K_FIRE  = PIN_BTN_FIRE;
constexpr uint8_t K_UP    = 28;
constexpr uint8_t K_DOWN  = 30;
constexpr uint8_t K_LEFT  = 32;
constexpr uint8_t K_RIGHT = 34;

#endif // PINOUT_H
