#pragma once

#include "katana_platform.h"

typedef struct {
        i32 x;
        i32 y;
} vec2_t;

typedef struct {
        unsigned char *tiles;
        u8 tile_width;
        u8 tile_height;
        u8 tiles_wide;
        u8 tiles_high;
} tilemap_t;

typedef struct {
        tilemap_t tilemap;
        u32 player_x;
        u32 player_y;
        u32 player_width;
        u32 player_height;
        u32 player_speed;

        f32 t_sine;
        i32 tone_hz;
} game_state_t;
