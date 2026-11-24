#ifndef HUD_H
#define HUD_H

#include <stdint.h>

// Modulo de HUD dedicado a faixa amarela superior do OLED Dual-Color (y = 0..15, paginas 0..1).
// Nenhum desenho ultrapassa y = 15, permitindo limpar o viewport 3D (paginas 2..7) sem flicker.

void hud_render_full(uint8_t health, uint8_t keys, uint8_t fps_val, bool sprinting, bool minimap_on);
void hud_update_vitals(uint8_t health, uint8_t keys);
void hud_update_stats(uint8_t fps_val, bool sprinting, bool minimap_on);

#endif // HUD_H
