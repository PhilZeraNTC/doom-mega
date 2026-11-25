#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>

// Diagnostico de travamento frame-a-frame via Serial (115200 baud) e LED_BUILTIN (D13)
// 0 = desligado (FPS cheio); 1 = log detalhado por etapa do loop
#define DEBUG_FREEZE        0

// Monitoramento leve de recuperacao de barramento I2C (zero overhead em operacao normal;
// imprime no Serial 115200 baud somente se twiRecoverBus() for acionada)
#define DEBUG_TWI_RECOVERY  1

// GFX settings
#define OPTIMIZE_SSD1306                // Optimizations for SSD1306 displays

#define FRAME_TIME          66.666666   // Desired time per frame in ms (66.666666 is ~15 fps)
#define RES_DIVIDER         2           // Higher values result in lower horizontal resolution and lower CPU/RAM usage
#define DISTANCE_MULTIPLIER 20          // Fixed-point scaling factor for entity/AI distance comparisons
#define MAX_RENDER_DEPTH    12
#define MAX_SPRITE_DEPTH    8

// Z-buffer de alta precisao (16 bits por coluna de tela, ponto fixo 8.8)
constexpr uint16_t Z_SCALE       = 256;
constexpr uint16_t Z_MAX         = 65534;
constexpr uint16_t Z_CLEAR       = 0xFFFF;
constexpr double   WALL_MIN_DIST = 0.15;

// Level
#define LEVEL_WIDTH_BASE    6
#define LEVEL_WIDTH         (1 << LEVEL_WIDTH_BASE)
#define LEVEL_HEIGHT        57
#define LEVEL_SIZE          (LEVEL_WIDTH / 2 * LEVEL_HEIGHT)
constexpr uint16_t LEVEL_BITMASK_BYTES = (LEVEL_WIDTH * LEVEL_HEIGHT + 7) / 8; // 456 bytes

// Scenes (Maquina de Estados Finitos)
#define INTRO                 0
#define GAME_PLAY             1
#define GAME_OVER             2
#define VICTORY               3

// Game
#define GUN_TARGET_POS        16        // Ajustado para proporcao do viewport azul de 48 px
#define GUN_SHOT_POS          (GUN_TARGET_POS + 4)

#define ROT_SPEED             0.12
#define MOV_SPEED             0.2
#define MOV_SPEED_INV         5         // 1 / MOV_SPEED
#define SPRINT_MULTIPLIER     1.5       // Multiplicador de velocidade com PIN_JOY_L_SW pressionado
#define MOV_ACCEL             0.6       // Resposta firme de aceleracao (0..1)
#define MOV_FRICTION          0.35      // Amortecimento rapido ao soltar o analogico (sem sensacao de gelo)
#define VEL_STOP_EPSILON      0.003

#define JOGGING_SPEED         0.005
#define ENEMY_SPEED           0.02
#define FIREBALL_SPEED        0.2
#define FIREBALL_ANGLES       45        // Num of angles per PI

#define MAX_ENTITIES          10        // Max num of active entities

#define MAX_ENTITY_DISTANCE   200       // * DISTANCE_MULTIPLIER
#define MAX_ENEMY_VIEW        80        // * DISTANCE_MULTIPLIER
#define ITEM_COLLIDER_DIST    6         // * DISTANCE_MULTIPLIER
#define ENEMY_COLLIDER_DIST   4         // * DISTANCE_MULTIPLIER
#define FIREBALL_COLLIDER_DIST 2        // * DISTANCE_MULTIPLIER
#define ENEMY_MELEE_DIST      6         // * DISTANCE_MULTIPLIER
#define WALL_COLLIDER_DIST    0.2

#define ENEMY_MELEE_DAMAGE    8
#define ENEMY_FIREBALL_DAMAGE 20
#define GUN_MAX_DAMAGE        15

// Display Dual-Color 128x64:
//   Paginas 0..1 (y = 0..15,  16 px): faixa superior AMARELA -> HUD
//   Paginas 2..7 (y = 16..63, 48 px): faixa inferior AZUL    -> Viewport 3D
constexpr uint8_t SCREEN_WIDTH   = 128;
constexpr uint8_t SCREEN_HEIGHT  = 64;
constexpr uint8_t HALF_WIDTH     = SCREEN_WIDTH / 2;
constexpr uint8_t HALF_HEIGHT    = SCREEN_HEIGHT / 2;

constexpr uint8_t HUD_HEIGHT     = 16;
constexpr uint8_t RENDER_TOP     = 16;
constexpr uint8_t RENDER_HEIGHT  = 48;
constexpr uint8_t RENDER_BOTTOM  = RENDER_TOP + RENDER_HEIGHT;       // 64
constexpr uint8_t RENDER_HORIZON = RENDER_TOP + (RENDER_HEIGHT / 2); // 40

#endif // CONSTANTS_H
