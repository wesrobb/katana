#pragma once

#include "katana_platform.h"

typedef struct {
        i32 x;
        i32 y;
} vec2_t;

typedef struct {
        f32 x;
        f32 y;
} vec2f_t;

typedef struct {
        unsigned char *tiles;
        f32 tile_width;
        f32 tile_height;
        u16 tiles_wide;
        u16 tiles_high;
} tilemap_t;

typedef struct {
        // Everything is measured in units, only during drawing
        // do we convert to pixels.
        f32 units_to_pixels;
        tilemap_t tilemap;
        vec2f_t player_pos;
        vec2f_t player_size;
        f32 player_speed;
        f32 gravity;
} world_t;

typedef struct {
        world_t world;

        f32 t_sine;
        i32 tone_hz;
} game_state_t;
