#pragma once

#include "katana_platform.h"

typedef struct {
        i32 x;
        i32 y;
} vec2i_t;

typedef struct {
        f32 x;
        f32 y;
} vec2f_t;

typedef struct {
        u32 size;
        u32 index;
        u8 *base;
} memory_arena_t;

void init_arena(memory_arena_t *arena, u32 size, u8 *base)
{
        arena->size = size;
        arena->base = base;
        arena->index = 0;
}

#define push_struct(arena, type) (type *) push_size_(arena, sizeof(type))
#define push_array(arena, count, type)                                         \
        (type *) push_size_(arena, (count) * sizeof(type))
void *push_size_(memory_arena_t *arena, u32 size)
{
        assert((arena->index + size) <= arena->size);
        void *result = arena->base + arena->index;
        arena->index += size;

        return result;
}

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
        u32 max_frames;
        u32 current_frame;
        u32 fps;
        f32 accumulator;
        image_t *frames;
} entity_anim_t;

typedef enum {
        entity_type_player,
        entity_type_teleporter,
} entity_type_t;

#define KATANA_MAX_HIT_ENTITIES 4
typedef struct {
        vec2f_t katana_offset; // Offset from the players position depending on
                               // which way the players is facing.
        u32 hit_entities[KATANA_MAX_HIT_ENTITIES];
        entity_anim_t walk;
        entity_anim_t attack;
        u32 teleporter_index;
        b8 attacking;
} entity_player_t;

typedef struct {
        image_t *image;
} entity_teleporter_t;

typedef struct {
        vec2f_t position;
        vec2f_t size;
        vec2f_t velocity;
        vec2f_t acceleration;

        f32 velocity_factor;
        f32 acceleration_factor;

        b8 on_ground;
        b8 exists;

        entity_type_t type;
        union {
                entity_player_t player;
                entity_teleporter_t teleporter;
        };
} entity_t;

typedef struct {
        vec2f_t position;
        f32 units_to_pixels;
} camera_t;

#define KATANA_MAX_ENTITIES 512
typedef struct {
        entity_t entities[KATANA_MAX_ENTITIES]; // Entity 0 is the "null" entity
        vec2f_t camera_tracked_positions[KATANA_MAX_ENTITIES]; // Camera will
                                                               // always ensure
                                                               // these
                                                               // positions are
                                                               // in view.
        u32 controlled_entities[KATANA_MAX_CONTROLLERS];
        tilemap_t tilemap;
        camera_t camera;
        vec2f_t gravity; // units/sec^2
} world_t;

typedef struct {
        world_t world;
        image_t background_image;
        image_t tile_image;
        image_t player_images[6];
        image_t player_attack_images[6];
        image_t green_teleporter;

        memory_arena_t arena;

        f32 t_sine;
        i32 tone_hz;
} game_state_t;
