#include "pinout.h"
#include "constants.h"
#include "level.h"
#include "sprites.h"
#include "input.h"
#include "entities.h"
#include "types.h"
#include "display.h"
#include "sound.h"

// Useful macros
#define swap_vals(a, b)       do { typeof(a) temp = (a); (a) = (b); (b) = temp; } while (0)
#define sign_dir(a, b)        ((double)((a) > (b) ? 1 : ((b) > (a) ? -1 : 0)))

// Explicit prototypes (prevents Arduino IDE auto-prototype issues with custom types/defaults)
void jumpTo(uint8_t target_scene);
void initializeLevel(const uint8_t level[]);
uint8_t getBlockAt(const uint8_t level[], uint8_t x, uint8_t y);
bool isSpawned(UID uid);
bool isStatic(UID uid);
void spawnEntity(uint8_t type, uint8_t x, uint8_t y);
void spawnFireball(double x, double y);
void removeEntity(UID uid, bool makeStatic = false);
void removeStaticEntity(UID uid);
UID detectCollision(const uint8_t level[], Coords *pos, double relative_x, double relative_y, bool only_walls = false);
void fire();
UID updatePosition(const uint8_t level[], Coords *pos, double relative_x, double relative_y, bool only_walls = false);
void updateEntities(const uint8_t level[]);
void renderMap(const uint8_t level[], double view_height);
void sortEntities();
Coords translateIntoView(Coords *pos);
void renderEntities(double view_height);
void renderGun(uint8_t gun_pos, double amount_jogging);
void renderHud();
void updateHud();
void renderStats();
void loopIntro();
void loopGamePlay();

// General state
uint8_t scene = INTRO;
bool exit_scene = false;
bool invert_screen = false;
uint8_t flash_screen = 0;

// Game entities
Player player;
Entity entity[MAX_ENTITIES];
StaticEntity static_entity[MAX_STATIC_ENTITIES];
uint8_t num_entities = 0;
uint8_t num_static_entities = 0;

void setup(void) {
  setupDisplay();
  input_setup();
  sound_init();
}

// Jump to another scene
void jumpTo(uint8_t target_scene) {
  scene = target_scene;
  exit_scene = true;
}

// Finds the player in the map
void initializeLevel(const uint8_t level[]) {
  for (int8_t y = LEVEL_HEIGHT - 1; y >= 0; y--) {
    for (uint8_t x = 0; x < LEVEL_WIDTH; x++) {
      uint8_t block = getBlockAt(level, x, (uint8_t)y);

      if (block == E_PLAYER) {
        player = create_player(x, y);
        return;
      }
    }
  }
}

uint8_t getBlockAt(const uint8_t level[], uint8_t x, uint8_t y) {
  if (x >= LEVEL_WIDTH || y >= LEVEL_HEIGHT) {
    return E_FLOOR;
  }

  // y is read in inverse order
  return (pgm_read_byte(level + (((LEVEL_HEIGHT - 1 - y) * LEVEL_WIDTH + x) / 2))
          >> (!(x % 2) * 4))
         & 0b1111;
}

bool isSpawned(UID uid) {
  for (uint8_t i = 0; i < num_entities; i++) {
    if (entity[i].uid == uid) return true;
  }

  return false;
}

bool isStatic(UID uid) {
  for (uint8_t i = 0; i < num_static_entities; i++) {
    if (static_entity[i].uid == uid) return true;
  }

  return false;
}

void spawnEntity(uint8_t type, uint8_t x, uint8_t y) {
  if (num_entities >= MAX_ENTITIES) {
    return;
  }

  switch (type) {
    case E_ENEMY:
      entity[num_entities] = create_enemy(x, y);
      num_entities++;
      break;

    case E_KEY:
      entity[num_entities] = create_key(x, y);
      num_entities++;
      break;

    case E_MEDIKIT:
      entity[num_entities] = create_medikit(x, y);
      num_entities++;
      break;
  }
}

void spawnFireball(double x, double y) {
  if (num_entities >= MAX_ENTITIES) {
    return;
  }

  UID uid = create_uid(E_FIREBALL, (uint8_t)x, (uint8_t)y);
  if (isSpawned(uid)) return;

  // Calculate direction (FIREBALL_ANGLES per PI)
  int16_t dir = FIREBALL_ANGLES + (int16_t)(atan2(y - player.pos.y, x - player.pos.x) / PI * FIREBALL_ANGLES);
  if (dir < 0) dir += FIREBALL_ANGLES * 2;
  entity[num_entities] = create_fireball((uint8_t)x, (uint8_t)y, (uint8_t)dir);
  num_entities++;
}

void removeEntity(UID uid, bool makeStatic) {
  (void)makeStatic;
  uint8_t i = 0;
  bool found = false;

  while (i < num_entities) {
    if (!found && entity[i].uid == uid) {
      found = true;
      num_entities--;
    }

    if (found && i < num_entities) {
      entity[i] = entity[i + 1];
    }

    i++;
  }
}

void removeStaticEntity(UID uid) {
  uint8_t i = 0;
  bool found = false;

  while (i < num_static_entities) {
    if (!found && static_entity[i].uid == uid) {
      found = true;
      num_static_entities--;
    }

    if (found && i < num_static_entities) {
      static_entity[i] = static_entity[i + 1];
    }

    i++;
  }
}

UID detectCollision(const uint8_t level[], Coords *pos, double relative_x, double relative_y, bool only_walls) {
  // Wall collision
  uint8_t round_x = (uint8_t)(int)(pos->x + relative_x);
  uint8_t round_y = (uint8_t)(int)(pos->y + relative_y);
  uint8_t block = getBlockAt(level, round_x, round_y);

  if (block == E_WALL) {
    playSound(hit_wall_snd, HIT_WALL_SND_LEN);
    return create_uid(block, round_x, round_y);
  }

  if (only_walls) {
    return UID_null;
  }

  // Entity collision
  for (uint8_t i = 0; i < num_entities; i++) {
    if (&(entity[i].pos) == pos) {
      continue;
    }

    uint8_t type = uid_get_type(entity[i].uid);

    // Only ALIVE enemy collision
    if (type != E_ENEMY || entity[i].state == S_DEAD || entity[i].state == S_HIDDEN) {
      continue;
    }

    Coords new_coords = { entity[i].pos.x - relative_x, entity[i].pos.y - relative_y };
    uint8_t distance = coords_distance(pos, &new_coords);

    if (distance < ENEMY_COLLIDER_DIST && distance < entity[i].distance) {
      return entity[i].uid;
    }
  }

  return UID_null;
}

// Shoot
void fire() {
  playSound(shoot_snd, SHOOT_SND_LEN);

  for (uint8_t i = 0; i < num_entities; i++) {
    if (uid_get_type(entity[i].uid) != E_ENEMY || entity[i].state == S_DEAD || entity[i].state == S_HIDDEN) {
      continue;
    }

    Coords transform = translateIntoView(&(entity[i].pos));
    if (abs(transform.x) < 20 && transform.y > 0) {
      double denom = abs(transform.x) * (double)entity[i].distance;
      uint8_t damage = (denom > 0.0)
                         ? (uint8_t)min((double)GUN_MAX_DAMAGE, (double)GUN_MAX_DAMAGE / denom / 5.0)
                         : GUN_MAX_DAMAGE;
      if (damage > 0) {
        entity[i].health = (uint8_t)max(0, (int)entity[i].health - (int)damage);
        entity[i].state = S_HIT;
        entity[i].timer = 4;
      }
    }
  }
}

// Update coords if possible. Return the collided uid, if any
UID updatePosition(const uint8_t level[], Coords *pos, double relative_x, double relative_y, bool only_walls) {
  UID collide_x = detectCollision(level, pos, relative_x, 0, only_walls);
  UID collide_y = detectCollision(level, pos, 0, relative_y, only_walls);

  if (!collide_x) pos->x += relative_x;
  if (!collide_y) pos->y += relative_y;

  return collide_x ? collide_x : collide_y;
}

void updateEntities(const uint8_t level[]) {
  uint8_t i = 0;
  while (i < num_entities) {
    entity[i].distance = coords_distance(&(player.pos), &(entity[i].pos));

    if (entity[i].timer > 0) entity[i].timer--;

    if (entity[i].distance > MAX_ENTITY_DISTANCE) {
      removeEntity(entity[i].uid);
      continue;
    }

    if (entity[i].state == S_HIDDEN) {
      i++;
      continue;
    }

    uint8_t type = uid_get_type(entity[i].uid);

    switch (type) {
      case E_ENEMY: {
        if (entity[i].health == 0) {
          if (entity[i].state != S_DEAD) {
            entity[i].state = S_DEAD;
            entity[i].timer = 6;
          }
        } else if (entity[i].state == S_HIT) {
          if (entity[i].timer == 0) {
            entity[i].state = S_ALERT;
            entity[i].timer = 40;
          }
        } else if (entity[i].state == S_FIRING) {
          if (entity[i].timer == 0) {
            entity[i].state = S_ALERT;
            entity[i].timer = 40;
          }
        } else {
          if (entity[i].distance > ENEMY_MELEE_DIST && entity[i].distance < MAX_ENEMY_VIEW) {
            if (entity[i].state != S_ALERT) {
              entity[i].state = S_ALERT;
              entity[i].timer = 20;
            } else {
              if (entity[i].timer == 0) {
                spawnFireball(entity[i].pos.x, entity[i].pos.y);
                entity[i].state = S_FIRING;
                entity[i].timer = 6;
              } else {
                updatePosition(
                  level,
                  &(entity[i].pos),
                  sign_dir(player.pos.x, entity[i].pos.x) * ENEMY_SPEED * delta,
                  sign_dir(player.pos.y, entity[i].pos.y) * ENEMY_SPEED * delta,
                  true
                );
              }
            }
          } else if (entity[i].distance <= ENEMY_MELEE_DIST) {
            if (entity[i].state != S_MELEE) {
              entity[i].state = S_MELEE;
              entity[i].timer = 10;
            } else if (entity[i].timer == 0) {
              player.health = (uint8_t)max(0, (int)player.health - ENEMY_MELEE_DAMAGE);
              entity[i].timer = 14;
              flash_screen = 1;
              updateHud();
            }
          } else {
            entity[i].state = S_STAND;
          }
        }
        break;
      }

      case E_FIREBALL: {
        if (entity[i].distance < FIREBALL_COLLIDER_DIST) {
          player.health = (uint8_t)max(0, (int)player.health - ENEMY_FIREBALL_DAMAGE);
          flash_screen = 1;
          updateHud();
          removeEntity(entity[i].uid);
          continue;
        } else {
          // Note: health stores the movement angle for fireballs
          UID collided = updatePosition(
            level,
            &(entity[i].pos),
            cos((double)entity[i].health / FIREBALL_ANGLES * PI) * FIREBALL_SPEED,
            sin((double)entity[i].health / FIREBALL_ANGLES * PI) * FIREBALL_SPEED,
            true
          );

          if (collided) {
            removeEntity(entity[i].uid);
            continue;
          }
        }
        break;
      }

      case E_MEDIKIT: {
        if (entity[i].distance < ITEM_COLLIDER_DIST) {
          playSound(medkit_snd, MEDKIT_SND_LEN);
          entity[i].state = S_HIDDEN;
          player.health = (uint8_t)min(100, (int)player.health + 50);
          updateHud();
          flash_screen = 1;
        }
        break;
      }

      case E_KEY: {
        if (entity[i].distance < ITEM_COLLIDER_DIST) {
          playSound(get_key_snd, GET_KEY_SND_LEN);
          entity[i].state = S_HIDDEN;
          player.keys++;
          updateHud();
          flash_screen = 1;
        }
        break;
      }
    }

    i++;
  }
}

// The map raycaster. Based on https://lodev.org/cgtutor/raycasting.html
void renderMap(const uint8_t level[], double view_height) {
  UID last_uid = UID_null;

  for (uint8_t x = 0; x < SCREEN_WIDTH; x += RES_DIVIDER) {
    double camera_x = 2.0 * (double)x / SCREEN_WIDTH - 1.0;
    double ray_x = player.dir.x + player.plane.x * camera_x;
    double ray_y = player.dir.y + player.plane.y * camera_x;
    uint8_t map_x = (uint8_t)(player.pos.x);
    uint8_t map_y = (uint8_t)(player.pos.y);
    Coords map_coords = { player.pos.x, player.pos.y };
    double delta_x = abs(1.0 / ray_x);
    double delta_y = abs(1.0 / ray_y);

    int8_t step_x;
    int8_t step_y;
    double side_x;
    double side_y;

    if (ray_x < 0) {
      step_x = -1;
      side_x = (player.pos.x - map_x) * delta_x;
    } else {
      step_x = 1;
      side_x = (map_x + 1.0 - player.pos.x) * delta_x;
    }

    if (ray_y < 0) {
      step_y = -1;
      side_y = (player.pos.y - map_y) * delta_y;
    } else {
      step_y = 1;
      side_y = (map_y + 1.0 - player.pos.y) * delta_y;
    }

    // Wall detection
    uint8_t depth = 0;
    bool hit = false;
    bool side = false;
    while (!hit && depth < MAX_RENDER_DEPTH) {
      if (side_x < side_y) {
        side_x += delta_x;
        map_x = (uint8_t)(map_x + step_x);
        side = false;
      } else {
        side_y += delta_y;
        map_y = (uint8_t)(map_y + step_y);
        side = true;
      }

      uint8_t block = getBlockAt(level, map_x, map_y);

      if (block == E_WALL) {
        hit = true;
      } else {
        if (block == E_ENEMY || (block & 0b00001000)) {
          if (coords_distance(&(player.pos), &map_coords) < MAX_ENTITY_DISTANCE) {
            UID uid = create_uid(block, map_x, map_y);
            if (last_uid != uid && !isSpawned(uid)) {
              spawnEntity(block, map_x, map_y);
              last_uid = uid;
            }
          }
        }
      }

      depth++;
    }

    if (hit) {
      double distance;

      if (!side) {
        distance = max(1.0, (map_x - player.pos.x + (1 - step_x) / 2.0) / ray_x);
      } else {
        distance = max(1.0, (map_y - player.pos.y + (1 - step_y) / 2.0) / ray_y);
      }

      zbuffer[x / Z_RES_DIVIDER] = (uint8_t)min(distance * DISTANCE_MULTIPLIER, 255.0);

      uint8_t line_height = (uint8_t)(RENDER_HEIGHT / distance);

      int16_t intensity = GRADIENT_COUNT - (int)(distance / MAX_RENDER_DEPTH * GRADIENT_COUNT) - (side ? 2 : 0);
      if (intensity < 0) intensity = 0;

      drawVLine(
        x,
        (int8_t)(view_height / distance - line_height / 2 + RENDER_HEIGHT / 2),
        (int8_t)(view_height / distance + line_height / 2 + RENDER_HEIGHT / 2),
        (uint8_t)intensity
      );
    }
  }
}

// Sort entities from far to close (comb sort)
void sortEntities() {
  uint8_t gap = num_entities;
  bool swapped = false;
  while (gap > 1 || swapped) {
    gap = (uint8_t)((gap * 10) / 13);
    if (gap == 9 || gap == 10) gap = 11;
    if (gap < 1) gap = 1;
    swapped = false;
    for (uint8_t i = 0; i + gap < num_entities; i++) {
      uint8_t j = i + gap;
      if (entity[i].distance < entity[j].distance) {
        swap_vals(entity[i], entity[j]);
        swapped = true;
      }
    }
  }
}

Coords translateIntoView(Coords *pos) {
  double sprite_x = pos->x - player.pos.x;
  double sprite_y = pos->y - player.pos.y;

  double inv_det = 1.0 / (player.plane.x * player.dir.y - player.dir.x * player.plane.y);
  double transform_x = inv_det * (player.dir.y * sprite_x - player.dir.x * sprite_y);
  double transform_y = inv_det * (-player.plane.y * sprite_x + player.plane.x * sprite_y);

  return { transform_x, transform_y };
}

void renderEntities(double view_height) {
  sortEntities();

  for (uint8_t i = 0; i < num_entities; i++) {
    if (entity[i].state == S_HIDDEN) continue;

    Coords transform = translateIntoView(&(entity[i].pos));

    if (transform.y <= 0.1 || transform.y > MAX_SPRITE_DEPTH) {
      continue;
    }

    int16_t sprite_screen_x = (int16_t)(HALF_WIDTH * (1.0 + transform.x / transform.y));
    int8_t sprite_screen_y = (int8_t)(RENDER_HEIGHT / 2 + view_height / transform.y);
    uint8_t type = uid_get_type(entity[i].uid);

    if (sprite_screen_x < -(int16_t)HALF_WIDTH || sprite_screen_x > (int16_t)SCREEN_WIDTH + (int16_t)HALF_WIDTH) {
      continue;
    }

    switch (type) {
      case E_ENEMY: {
        uint8_t sprite;
        if (entity[i].state == S_ALERT) {
          sprite = (uint8_t)((millis() / 500) % 2);
        } else if (entity[i].state == S_FIRING) {
          sprite = 2;
        } else if (entity[i].state == S_HIT) {
          sprite = 3;
        } else if (entity[i].state == S_MELEE) {
          sprite = entity[i].timer > 10 ? 2 : 1;
        } else if (entity[i].state == S_DEAD) {
          sprite = entity[i].timer > 0 ? 3 : 4;
        } else {
          sprite = 0;
        }

        drawSprite(
          (int8_t)(sprite_screen_x - BMP_IMP_WIDTH * 0.5 / transform.y),
          (int8_t)(sprite_screen_y - 8 / transform.y),
          bmp_imp_bits,
          bmp_imp_mask,
          BMP_IMP_WIDTH,
          BMP_IMP_HEIGHT,
          sprite,
          transform.y
        );
        break;
      }

      case E_FIREBALL: {
        drawSprite(
          (int8_t)(sprite_screen_x - BMP_FIREBALL_WIDTH / 2 / transform.y),
          (int8_t)(sprite_screen_y - BMP_FIREBALL_HEIGHT / 2 / transform.y),
          bmp_fireball_bits,
          bmp_fireball_mask,
          BMP_FIREBALL_WIDTH,
          BMP_FIREBALL_HEIGHT,
          0,
          transform.y
        );
        break;
      }

      case E_MEDIKIT: {
        drawSprite(
          (int8_t)(sprite_screen_x - BMP_ITEMS_WIDTH / 2 / transform.y),
          (int8_t)(sprite_screen_y + 5 / transform.y),
          bmp_items_bits,
          bmp_items_mask,
          BMP_ITEMS_WIDTH,
          BMP_ITEMS_HEIGHT,
          0,
          transform.y
        );
        break;
      }

      case E_KEY: {
        drawSprite(
          (int8_t)(sprite_screen_x - BMP_ITEMS_WIDTH / 2 / transform.y),
          (int8_t)(sprite_screen_y + 5 / transform.y),
          bmp_items_bits,
          bmp_items_mask,
          BMP_ITEMS_WIDTH,
          BMP_ITEMS_HEIGHT,
          1,
          transform.y
        );
        break;
      }
    }
  }
}

void renderGun(uint8_t gun_pos, double amount_jogging) {
  int8_t x = (int8_t)(48 + sin((double)millis() * JOGGING_SPEED) * 10 * amount_jogging);
  int8_t y = (int8_t)(RENDER_HEIGHT - gun_pos + abs(cos((double)millis() * JOGGING_SPEED)) * 8 * amount_jogging);

  if (gun_pos > GUN_SHOT_POS - 2) {
    display.drawBitmap(x + 6, y - 11, bmp_fire_bits, BMP_FIRE_WIDTH, BMP_FIRE_HEIGHT, 1);
  }

  uint8_t clip_height = (uint8_t)max(0, min((int)y + BMP_GUN_HEIGHT, (int)RENDER_HEIGHT) - (int)y);

  display.drawBitmap(x, y, bmp_gun_mask, BMP_GUN_WIDTH, clip_height, 0);
  display.drawBitmap(x, y, bmp_gun_bits, BMP_GUN_WIDTH, clip_height, 1);
}

// Only needed first time
void renderHud() {
  drawText(2, 58, F("{}"), 0);        // Health symbol
  drawText(40, 58, F("[]"), 0);       // Keys symbol
  updateHud();
}

// Render values for the HUD
void updateHud() {
  display.clearRect(12, 58, 15, 6);
  display.clearRect(50, 58, 5, 6);

  drawText((uint8_t)12, (uint8_t)58, player.health);
  drawText((uint8_t)50, (uint8_t)58, player.keys);
}

// Debug stats
void renderStats() {
  display.clearRect(58, 58, 70, 6);
  drawText((uint8_t)114, (uint8_t)58, (uint8_t)getActualFps());
  drawText((uint8_t)82, (uint8_t)58, num_entities);
}

// Intro screen
void loopIntro() {
  display.drawBitmap(
    (SCREEN_WIDTH - BMP_LOGO_WIDTH) / 2,
    (SCREEN_HEIGHT - BMP_LOGO_HEIGHT) / 3,
    bmp_logo_bits,
    BMP_LOGO_WIDTH,
    BMP_LOGO_HEIGHT,
    1
  );

  delay(1000);
  drawText((int8_t)(SCREEN_WIDTH / 2 - 25), (int8_t)(SCREEN_HEIGHT * 0.8), F("PRESS FIRE"));
  display.display();

  // Sincroniza estado de botoes apos o delay antes de aguardar borda de disparo
  input_update();

  while (!exit_scene) {
    input_update();
    if (input_fire_just_pressed()) {
      jumpTo(GAME_PLAY);
    }
  }
}

void loopGamePlay() {
  bool walkSoundToggle = false;
  uint8_t gun_pos = 0;
  double rot_speed = 0;
  double old_dir_x = 0;
  double old_plane_x = 0;
  double view_height = 0;
  double jogging = 0;
  uint8_t fade = GRADIENT_COUNT - 1;

  // Reset per-game state cleanly
  num_entities = 0;
  num_static_entities = 0;
  flash_screen = 0;
  invert_screen = false;

  initializeLevel(sto_level_1);
  input_update(); // Descarta qualquer borda pendente vinda da tela anterior
  lastFrameTime = millis();

  do {
    fps();
    input_update();

    // Clear only the 3d view
    memset(display_buf, 0, SCREEN_WIDTH * (RENDER_HEIGHT / 8));

    if (player.health > 0) {
      // Leitura proporcional frente/tras + strafe lateral com normalizacao diagonal
      double move_fwd = (double)input_move_forward();
      double move_str = (double)input_move_strafe();
      double move_mag_sq = move_fwd * move_fwd + move_str * move_str;
      if (move_mag_sq > 1.0) {
        double inv_mag = 1.0 / sqrt(move_mag_sq);
        move_fwd *= inv_mag;
        move_str *= inv_mag;
      }

      // Sprint (clique esquerdo SW pressionado enquanto o jogador se move)
      double max_speed = MOV_SPEED;
      if (input_sprint() && move_mag_sq > 0.0) {
        max_speed *= SPRINT_MULTIPLIER;
      }

      double target_vel    = move_fwd * max_speed;
      double target_strafe = move_str * max_speed;

      // Resposta firme sem excesso de inercia ("gelo")
      if (move_fwd != 0.0) {
        player.velocity += (target_vel - player.velocity) * MOV_ACCEL;
      } else {
        player.velocity *= MOV_FRICTION;
      }

      if (move_str != 0.0) {
        player.strafe_velocity += (target_strafe - player.strafe_velocity) * MOV_ACCEL;
      } else {
        player.strafe_velocity *= MOV_FRICTION;
      }

      if (abs(player.velocity) < VEL_STOP_EPSILON) {
        player.velocity = 0.0;
      }
      if (abs(player.strafe_velocity) < VEL_STOP_EPSILON) {
        player.strafe_velocity = 0.0;
      }

      // Magnitude total da velocidade para head-bobbing (jogging)
      double vel_sq = player.velocity * player.velocity + player.strafe_velocity * player.strafe_velocity;
      if (vel_sq > 0.0) {
        jogging = sqrt(vel_sq) * MOV_SPEED_INV;
        if (jogging > 1.35) jogging = 1.35;
      } else {
        jogging = 0.0;
      }

      // Rotacao proporcional da camera (pula cos/sin quando o analogico esta neutro)
      // Convencao: dir=(1,0), plane=(0,-0.66) => turn > 0 (direita) usa angulo negativo
      float turn = input_turn();
      if (turn != 0.0f) {
        rot_speed = -(double)turn * ROT_SPEED * delta;
        double cos_r = cos(rot_speed);
        double sin_r = sin(rot_speed);

        old_dir_x = player.dir.x;
        player.dir.x = player.dir.x * cos_r - player.dir.y * sin_r;
        player.dir.y = old_dir_x * sin_r + player.dir.y * cos_r;

        old_plane_x = player.plane.x;
        player.plane.x = player.plane.x * cos_r - player.plane.y * sin_r;
        player.plane.y = old_plane_x * sin_r + player.plane.y * cos_r;
      }

      view_height = abs(sin((double)millis() * JOGGING_SPEED)) * 6 * jogging;

      if (view_height > 5.9) {
        if (!sound) {
          if (walkSoundToggle) {
            playSound(walk1_snd, WALK1_SND_LEN);
            walkSoundToggle = false;
          } else {
            playSound(walk2_snd, WALK2_SND_LEN);
            walkSoundToggle = true;
          }
        }
      }

      // Controle de disparo por borda (evita tiro acidental ao segurar gatilho na transicao)
      if (gun_pos > GUN_TARGET_POS) {
        gun_pos -= 1;
      } else if (gun_pos < GUN_TARGET_POS) {
        gun_pos += 2;
      } else if (input_fire_just_pressed()) {
        gun_pos = GUN_SHOT_POS;
        fire();
      }

      // Aplicacao do movimento vetorial:
      //   frente  = (+dir.x, +dir.y)
      //   direita = (+dir.y, -dir.x) (mesmo sentido de player.plane)
      if (player.velocity != 0.0 || player.strafe_velocity != 0.0) {
        double move_dx = (player.dir.x * player.velocity + player.dir.y * player.strafe_velocity) * delta;
        double move_dy = (player.dir.y * player.velocity - player.dir.x * player.strafe_velocity) * delta;
        updatePosition(sto_level_1, &(player.pos), move_dx, move_dy);
      }
    } else {
      // Jogador morto: interrompe imediatamente qualquer deslizamento residual
      player.velocity = 0.0;
      player.strafe_velocity = 0.0;
      jogging = 0.0;

      if (view_height > -10) {
        view_height--;
      } else if (input_fire_just_pressed()) {
        jumpTo(INTRO);
      }

      if (gun_pos > 1) {
        gun_pos -= 2;
      }
    }

    updateEntities(sto_level_1);

    renderMap(sto_level_1, view_height);
    renderEntities(view_height);
    renderGun(gun_pos, jogging);

    if (fade > 0) {
      fadeScreen(fade);
      fade--;

      if (fade == 0) {
        renderHud();
      }
    } else {
      renderStats();
    }

    if (flash_screen > 0) {
      invert_screen = !invert_screen;
      flash_screen--;
    } else if (invert_screen) {
      invert_screen = false;
    }

    display.invertDisplay(invert_screen);
    display.display();
  } while (!exit_scene);
}

void loop(void) {
  switch (scene) {
    case INTRO: {
      loopIntro();
      break;
    }
    case GAME_PLAY: {
      loopGamePlay();
      break;
    }
  }

  for (uint8_t i = 0; i < GRADIENT_COUNT; i++) {
    fadeScreen(i, false);
    display.display();
    delay(40);
  }
  exit_scene = false;
}
