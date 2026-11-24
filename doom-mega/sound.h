/*
 * Non-blocking PC-Speaker-style Inverse Frequency Sound driver
 * Timer1 CTC hardware toggle on OC1A (PIN_BUZZER = D11 on ATmega2560)
 * Timer2 CTC ISR at ~139.5 Hz sample rate
 */

#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>
#include <avr/pgmspace.h>

// Niveis de prioridade para evitar que passos/parede cortem efeitos importantes
constexpr uint8_t SND_PRIO_LOW      = 0; // Passos, batida em parede
constexpr uint8_t SND_PRIO_NORMAL   = 1; // Tiro, fireball, clique de UI
constexpr uint8_t SND_PRIO_HIGH     = 2; // Acerto em inimigo, dano no jogador, coleta
constexpr uint8_t SND_PRIO_CRITICAL = 3; // Game Over, Vitoria

constexpr uint8_t GET_KEY_SND_LEN     = 90;
constexpr uint8_t SHOOT_SND_LEN       = 27;
constexpr uint8_t HIT_WALL_SND_LEN    = 8;
constexpr uint8_t WALK1_SND_LEN       = 3;
constexpr uint8_t WALK2_SND_LEN       = 3;
constexpr uint8_t MEDKIT_SND_LEN      = 71;

constexpr uint8_t ENEMY_HIT_SND_LEN   = 14;
constexpr uint8_t PLAYER_HURT_SND_LEN = 18;
constexpr uint8_t FIREBALL_SND_LEN    = 15;
constexpr uint8_t UI_CLICK_SND_LEN    = 4;
constexpr uint8_t GAME_OVER_SND_LEN   = 52;
constexpr uint8_t VICTORY_SND_LEN     = 54;

extern const uint8_t shoot_snd[SHOOT_SND_LEN] PROGMEM;
extern const uint8_t get_key_snd[GET_KEY_SND_LEN] PROGMEM;
extern const uint8_t hit_wall_snd[HIT_WALL_SND_LEN] PROGMEM;
extern const uint8_t walk1_snd[WALK1_SND_LEN] PROGMEM;
extern const uint8_t walk2_snd[WALK2_SND_LEN] PROGMEM;
extern const uint8_t medkit_snd[MEDKIT_SND_LEN] PROGMEM;

extern const uint8_t enemy_hit_snd[ENEMY_HIT_SND_LEN] PROGMEM;
extern const uint8_t player_hurt_snd[PLAYER_HURT_SND_LEN] PROGMEM;
extern const uint8_t fireball_snd[FIREBALL_SND_LEN] PROGMEM;
extern const uint8_t ui_click_snd[UI_CLICK_SND_LEN] PROGMEM;
extern const uint8_t game_over_snd[GAME_OVER_SND_LEN] PROGMEM;
extern const uint8_t victory_snd[VICTORY_SND_LEN] PROGMEM;

extern volatile bool sound;

void sound_init();
void playSound(const uint8_t *snd, uint8_t len, uint8_t priority = SND_PRIO_NORMAL);
void sound_off();

#endif // SOUND_H
