#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>

// GFX settings
#define OPTIMIZE_SSD1306                // Optimizations for SSD1306 displays

#define FRAME_TIME          66.666666   // Desired time per frame in ms (66.666666 is ~15 fps)
#define RES_DIVIDER         2           // Higher values result in lower horizontal resolution and lower CPU/RAM usage
#define Z_RES_DIVIDER       2           // Z-buffer resolution divider
#define DISTANCE_MULTIPLIER 20          // Fixed-point scaling factor for uint8_t z-buffer (max 256 / MAX_RENDER_DEPTH)
#define MAX_RENDER_DEPTH    12
#define MAX_SPRITE_DEPTH    8

#define ZBUFFER_SIZE        (SCREEN_WIDTH / Z_RES_DIVIDER)

// Level
#define LEVEL_WIDTH_BASE    6
#define LEVEL_WIDTH         (1 << LEVEL_WIDTH_BASE)
#define LEVEL_HEIGHT        57
#define LEVEL_SIZE          (LEVEL_WIDTH / 2 * LEVEL_HEIGHT)

// Scenes
#define INTRO                 0
#define GAME_PLAY             1

// Game
#define GUN_TARGET_POS        18
#define GUN_SHOT_POS          (GUN_TARGET_POS + 4)

#define ROT_SPEED             0.12
#define MOV_SPEED             0.2
#define MOV_SPEED_INV         5         // 1 / MOV_SPEED

#define JOGGING_SPEED         0.005
#define ENEMY_SPEED           0.02
#define FIREBALL_SPEED        0.2
#define FIREBALL_ANGLES       45        // Num of angles per PI

#define MAX_ENTITIES          10        // Max num of active entities
#define MAX_STATIC_ENTITIES   28        // Max num of entities in sleep mode

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

// Display
constexpr uint8_t SCREEN_WIDTH  = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr uint8_t HALF_WIDTH    = SCREEN_WIDTH / 2;
constexpr uint8_t RENDER_HEIGHT = 56;   // Raycaster working height (remaining 8 px for HUD)
constexpr uint8_t HALF_HEIGHT   = SCREEN_HEIGHT / 2;

#endif // CONSTANTS_H
