/*
 * Non-blocking audio driver using Timer1 (OC1A -> D11 on ATmega2560)
 * and Timer2 CTC ISR (~139.5 Hz).
 */

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include "pinout.h"
#include "sound.h"

const uint8_t shoot_snd[SHOOT_SND_LEN] PROGMEM = {
  0x10, 0x10, 0x10, 0x6e, 0x2a, 0x20, 0x28, 0x28, 0x9b,
  0x28, 0x20, 0x20, 0x21, 0x57, 0x20, 0x20, 0x20, 0x67,
  0x20, 0x20, 0x29, 0x20, 0x73, 0x20, 0x20, 0x20, 0x89
};

const uint8_t get_key_snd[GET_KEY_SND_LEN] PROGMEM = {
  0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
  0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
  0x24, 0x24, 0x24, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37,
  0x20, 0x20, 0x20, 0x20, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37,
  0x37, 0x37, 0x37, 0x37, 0x20, 0x20, 0x20, 0x20, 0x37, 0x37,
  0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
  0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19
};

const uint8_t hit_wall_snd[HIT_WALL_SND_LEN] PROGMEM = {
  0x83, 0x83, 0x82, 0x8e, 0x8a, 0x89, 0x86, 0x84
};

const uint8_t walk1_snd[WALK1_SND_LEN] PROGMEM = {
  0x8f, 0x8e, 0x8e
};

const uint8_t walk2_snd[WALK2_SND_LEN] PROGMEM = {
  0x84, 0x87, 0x84
};

const uint8_t medkit_snd[MEDKIT_SND_LEN] PROGMEM = {
  0x55, 0x20, 0x3a, 0x3a, 0x3a, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x33, 0x33, 0x33, 0x33, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x26, 0x26, 0x26, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x16, 0x16, 0x16, 0x16, 0x16,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x16,
  0x16, 0x16, 0x16, 0x16, 0x16, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
  0x15
};

static volatile uint8_t  idx     = 0;
volatile bool            sound   = false;
static volatile uint16_t snd_ptr = 0;
static volatile uint8_t  snd_len = 0;

static void setFrequency(uint16_t freq) {
  if (freq == 0) {
    sound_off();
    return;
  }

  uint32_t requiredDivisor = (F_CPU / 2UL) / (uint32_t)freq;
  uint16_t prescalerVal;
  uint8_t  prescalerBits;

  if (requiredDivisor < 65536UL) {
    prescalerVal  = 1;
    prescalerBits = 1; // prescaler 1
  } else if (requiredDivisor < 8UL * 65536UL) {
    prescalerVal  = 8;
    prescalerBits = 2; // prescaler 8
  } else if (requiredDivisor < 64UL * 65536UL) {
    prescalerVal  = 64;
    prescalerBits = 3; // prescaler 64
  } else if (requiredDivisor < 256UL * 65536UL) {
    prescalerVal  = 256;
    prescalerBits = 4; // prescaler 256
  } else {
    prescalerVal  = 1024;
    prescalerBits = 5; // prescaler 1024
  }

  uint32_t calc = (requiredDivisor + (prescalerVal / 2U)) / prescalerVal;
  uint16_t top  = (calc > 0) ? (uint16_t)(calc - 1UL) : 0;

  TCCR1A = _BV(COM1A0);                   // Toggle OC1A (PIN_BUZZER = D11 on Mega 2560)
  TCCR1B = (1 << WGM12) | prescalerBits;  // CTC mode (OCR1A as TOP)
  TCCR1C = _BV(FOC1A);
  if (TCNT1 > top) {
    TCNT1 = 0;
  }
  OCR1A = top;
}

void sound_off() {
  TCCR1A = 0;
  TCCR1B = 0;
  digitalWrite(PIN_BUZZER, LOW); // Ensure no DC current remains on passive buzzer
}

void sound_init() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    sound_off();
    idx     = 0;
    sound   = false;
    snd_ptr = 0;
    snd_len = 0;

    TCCR2A = (1 << WGM21);                             // CTC mode
    TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);  // prescaler 1024
    TCNT2  = 0;
    OCR2A  = 112 - 1;                                  // 16 MHz / 1024 / 112 ~= 139.5 Hz
    TIMSK2 = (1 << OCIE2A);                            // Enable Timer2 Compare Match A ISR
  }
}

void playSound(const uint8_t *snd, uint8_t len) {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    snd_ptr = reinterpret_cast<uint16_t>(snd);
    snd_len = len;
    idx     = 0;
    sound   = (snd != nullptr && len > 0);
  }
}

ISR(TIMER2_COMPA_vect) {
  if (sound) {
    if (idx < snd_len) {
      uint8_t sample = pgm_read_byte(snd_ptr + idx);
      idx++;
      if (sample == 0) {
        sound_off();
      } else {
        uint16_t freq = (uint16_t)(1192030UL / (60UL * (uint32_t)sample));
        setFrequency(freq);
      }
    } else {
      idx = 0;
      sound_off();
      sound = false;
    }
  }
}
