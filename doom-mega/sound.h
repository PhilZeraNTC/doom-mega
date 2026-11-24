/*
 * Non-blocking PC-Speaker-style Inverse Frequency Sound driver
 * Timer1 CTC hardware toggle on OC1A (PIN_BUZZER = D11 on ATmega2560)
 * Timer2 CTC ISR at ~139.5 Hz sample rate
 */

#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>
#include <avr/pgmspace.h>

constexpr uint8_t GET_KEY_SND_LEN  = 90;
constexpr uint8_t SHOOT_SND_LEN    = 27;
constexpr uint8_t HIT_WALL_SND_LEN = 8;
constexpr uint8_t WALK1_SND_LEN    = 3;
constexpr uint8_t WALK2_SND_LEN    = 3;
constexpr uint8_t MEDKIT_SND_LEN   = 71;

extern const uint8_t shoot_snd[SHOOT_SND_LEN] PROGMEM;
extern const uint8_t get_key_snd[GET_KEY_SND_LEN] PROGMEM;
extern const uint8_t hit_wall_snd[HIT_WALL_SND_LEN] PROGMEM;
extern const uint8_t walk1_snd[WALK1_SND_LEN] PROGMEM;
extern const uint8_t walk2_snd[WALK2_SND_LEN] PROGMEM;
extern const uint8_t medkit_snd[MEDKIT_SND_LEN] PROGMEM;

extern volatile bool sound;

void sound_init();
void playSound(const uint8_t *snd, uint8_t len);
void sound_off();

#endif // SOUND_H
