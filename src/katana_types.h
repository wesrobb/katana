#pragma once

#include "katana_platform.h"

#include <immintrin.h>

typedef struct {
        i32 x;
        i32 y;
} vec2i_t;

typedef struct {
        f32 x;
        f32 y;
} vec2f_t;

typedef struct {
        u8 *data; // Order is always RGBA
        i32 width;
        i32 height;
} image_t;

typedef struct {
        unsigned char tiles[18 * 32];
        vec2f_t tile_size;
        u16 tiles_wide;
        u16 tiles_high;
} tilemap_t;

typedef struct {
        vec2f_t position;
        vec2f_t size;
        vec2f_t current_velocity;
        f32 max_speed;
        f32 acceleration;
        f32 jump_speed;
        i8 jump_count;
        b8 on_ground;
} player_t;

typedef struct {
        player_t player;
        tilemap_t tilemap;
        vec2f_t draw_offset;
        f32 gravity;
        // Everything is measured in units, only during drawing
        // do we convert to pixels.
        f32 units_to_pixels;
} world_t;

typedef struct {
        world_t world;
        image_t background_image;

        f32 t_sine;
        i32 tone_hz;
} game_state_t;
