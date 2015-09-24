#pragma once

#include "gg_platform.h"
#include "gg_vec.h"

// Note(Wes): Memory
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

#define push_struct(arena, type) (type *) push_size(arena, sizeof(type))
#define push_array(arena, count, type) (type *) push_size(arena, (count) * sizeof(type))
void *push_size(memory_arena_t *arena, u32 size)
{
    assert((arena->index + size) <= arena->size);
    void *result = arena->base + arena->index;
    arena->index += size;

    return result;
}

typedef struct {
    v2 pos;
    v2 size;
} rect_t;

typedef struct {
    u32 *data; // Always 4bpp. Order is always RGBA
    u32 w;
    u32 h;
} image_t;

typedef struct {
    unsigned char tiles[18 * 32];
    v2 tile_size;
    u16 tiles_wide;
    u16 tiles_high;
} tilemap_t;

// Note(Wes): Entities
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

#define GG_MAX_HIT_ENTITIES 4
typedef struct {
    v2 katana_offset; // Offset from the players position depending on
                      // which way the players is facing.
    u32 hit_entities[GG_MAX_HIT_ENTITIES];
    entity_anim_t walk;
    entity_anim_t attack;
    u32 teleporter_index;
    b8 attacking;
} entity_player_t;

typedef struct {
    image_t *image;
} entity_teleporter_t;

typedef struct {
    v2 position;
    v2 size;
    v2 velocity;
    v2 acceleration;

    f32 velocity_factor;
    f32 acceleration_factor;

    f32 rotation;

    b8 on_ground;
    b8 exists;

    entity_type_t type;
    union {
        entity_player_t player;
        entity_teleporter_t teleporter;
    };
} entity_t;

// Note(Wes): Renderer
typedef struct {
    v2 position;
    f32 units_to_pixels;
} camera_t;

typedef struct {
    v4 color;
    // Lights have a Z axis so that we know the angle the light hits the normal map.
    v3 position;
    v4 ambient; // TODO(Wes): This should not be stored per light.
    f32 radius;
} light_t;

typedef struct {
    v2 origin;
    v2 x_axis;
    v2 y_axis;
} basis_t;

typedef struct {
    camera_t *camera;

    u32 size;
    u32 index;
    u8 *base;
} render_queue_t;

// Note(Wes): Game
#define GG_MAX_ENTITIES 512
typedef struct {
    entity_t entities[GG_MAX_ENTITIES];           // Entity 0 is the "null" entity
    v2 camera_tracked_positions[GG_MAX_ENTITIES]; // Camera will
                                                  // always ensure
                                                  // these
                                                  // positions are
                                                  // in view.
    u32 controlled_entities[GG_MAX_CONTROLLERS];
    tilemap_t tilemap;
    camera_t camera;
    v2 gravity; // units/sec^2
} world_t;

typedef struct {
    world_t world;
    image_t background_image;
    image_t tile_image;
    image_t player_images[6];
    image_t player_normal;
    image_t player_attack_images[6];
    image_t green_teleporter;
    // TODO(Wes): Remove test image.
    image_t test_image;
    image_t test_image2;
    image_t test_image3;

    memory_arena_t arena;
    memory_arena_t frame_arena;

    render_queue_t *render_queue;

    f32 t_sine;
    i32 tone_hz;
    // TODO(Wes): Just for testing rotated rectangle rendering.
    f32 elapsed_time;
} game_state_t;
