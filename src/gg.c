#include "gg_types.h"
#include "gg_math.h"
#include "gg_vec.h"
#include "gg_aabb.h"
#include "gg_render.h"
#include "gg_collider.h"

#define STB_ASSERT(x) assert(x);
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <float.h>

static void output_sine_wave(game_state_t *game_state, game_audio_t *audio)
{
    // i16 tone_volume = 3000;
    i32 wave_period = audio->samples_per_second / game_state->tone_hz;

    i16 *sample_out = audio->samples;
    for (u32 i = 0; i < audio->sample_count; ++i) {
#if 0
                f32 sine_val = sinf(game_state->t_sine);
                i16 sample_val = (i16)(sine_val * tone_volume);
#else
        i16 sample_val = 0;
#endif
        *sample_out++ = sample_val;

        const f32 pi = 3.14159265359f;
        game_state->t_sine += 2.0f * pi * 1.0f / (f32)wave_period;
        if (game_state->t_sine > 2.0f * pi) {
            game_state->t_sine -= 2.0f * pi;
        }
    }
}

static unsigned char *get_tile(tilemap_t *tilemap, u32 x, u32 y)
{
    assert(tilemap);
    if (x >= tilemap->tiles_wide || y >= tilemap->tiles_high) {
        return 0;
    }

    return &tilemap->tiles[x + y * tilemap->tiles_wide];
}

static unsigned char *get_tile_from_world_pos(tilemap_t *tilemap, v2 pos)
{
    u32 tile_x = (u32)(pos.x / tilemap->tile_size.x);
    u32 tile_y = (u32)(pos.y / tilemap->tile_size.y);
    return get_tile(tilemap, tile_x, tile_y);
}

static u32 get_next_entity(entity_t *entities)
{
    static const entity_t zero_entity = {0};
    for (u32 i = 1; i < GG_MAX_ENTITIES; ++i) {
        entity_t *entity = &entities[i];
        if (!entity->exists) {
            *entity = zero_entity;
            entity->exists = 1;
            return i;
        }
    }

    return 0;
}

static void free_entity(entity_t *entities, u32 index)
{
    static const entity_t zero_entity = {0};
    assert(index < GG_MAX_ENTITIES);
    entities[index] = zero_entity;
}

static b8 is_tile_empty(unsigned char *tile)
{
    return *tile == 0;
}

static b8 test_wall(f32 test_x,
                    f32 test_y,
                    f32 wall_x,
                    f32 wall_y_min,
                    f32 wall_y_max,
                    f32 move_delta_x,
                    f32 move_delta_y,
                    f32 *t_min)
{
    if (move_delta_x == 0.0f) {
        return 0;
    }

    f32 t = (wall_x - test_x) / move_delta_x;
    if (t >= 0.0f && t < *t_min) {
        f32 y = test_y + t * move_delta_y;
        if (y >= wall_y_min && y <= wall_y_max) {
            const f32 t_epsilon = 0.001f;
            *t_min = t - t_epsilon;
            return 1;
        }
    }

    return 0;
}

static void update_entities(game_state_t *game_state, game_input_t *input, log_fn log)
{
    world_t *world = &game_state->world;

    // NOTE(Wes): Save input data from each controller.
    v2 new_accels[GG_MAX_ENTITIES] = {0};
    for (u32 i = 0; i < GG_MAX_CONTROLLERS; ++i) {
        u32 entity_index = world->controlled_entities[i];
        if (entity_index != 0) {
            entity_t *entity = &world->entities[entity_index];
            if (!entity->exists) {
                continue;
            }
            game_controller_input_t *controller = &input->controllers[i];
#if 0
            // Jump
            if (controller->action_down.ended_down && entity->on_ground) {
                entity->velocity.y = -80.0f;
            }

#endif
            // Move
            v2 left_stick = {0};
            if (controller->is_analog) {
                left_stick.x = controller->left_stick_x;
                left_stick.y = controller->left_stick_y;
            } else {
                if (controller->move_left.ended_down) {
                    left_stick.x = -1.0f;
                }
                if (controller->move_right.ended_down) {
                    left_stick.x = 1.0f;
                }
                if (controller->move_up.ended_down) {
                    left_stick.y = -1.0f;
                }
                if (controller->move_down.ended_down) {
                    left_stick.y = 1.0f;
                }
            }
            new_accels[entity_index] = v2_mul(left_stick, entity->acceleration_factor);
        }
    }

    for (u32 i = 1; i < GG_MAX_ENTITIES; ++i) {
        entity_t *entity = &world->entities[i];
        if (!entity->exists) {
            continue;
        }

        // Ensures we never move exactly onto the tile we are colliding
        // with.
        v2 new_accel = new_accels[i];

        // Gravity
        // new_accel = v2_add(new_accel, world->gravity);

        // Friction force. USE ODE here!
        v2 friction = v2_mul(entity->velocity, entity->velocity_factor);
        new_accel = v2_add(new_accel, friction);

// TODO(Wes): Decide which position calculation we prefer.
#if 1
        // Velocity verlet integration.
        v2 average_accel = v2_div(v2_add(entity->acceleration, new_accel), 2);
        v2 new_entity_pos = v2_mul(v2_mul(entity->acceleration, 0.5f), input->delta_time * input->delta_time);
        new_entity_pos = v2_add(v2_mul(entity->velocity, input->delta_time), new_entity_pos);
        new_entity_pos = v2_add(entity->position, new_entity_pos);
        entity->velocity = v2_add(v2_mul(average_accel, input->delta_time), entity->velocity);
        entity->acceleration = new_accel;
#else
        // NOTE(Wes): Euler integration
        v2 entity->position = entity->position;
        v2 new_entity_pos = v2_mul(v2_mul(entity->acceleration, 0.5f), input->delta_time * input->delta_time);
        new_entity_pos = v2_add(v2_mul(entity->velocity, input->delta_time), new_entity_pos);
        new_entity_pos = v2_add(entity->position, new_entity_pos);
        entity->velocity = v2_add(v2_mul(new_accel, input->delta_time), entity->velocity);
        entity->acceleration = new_accel;
#endif

        v2 pos_delta = v2_sub(new_entity_pos, entity->position);

        // NOTE(Wes): Collision detection
        //            Check that the intended position of the player is on a tile that is empty.
        //            Otherwise find the closest position to the edge of the tile and continue
        //            velocity from that point in a direction that is "safe".
        v2 min_pos = V2(kmin(new_entity_pos.x, entity->position.x), kmin(new_entity_pos.y, entity->position.y));
        v2 max_pos = V2(kmax(new_entity_pos.x + entity->size.x, entity->position.x + entity->size.x),
                        kmax(new_entity_pos.y + entity->size.y, entity->position.y + entity->size.y));

        tilemap_t *tilemap = &world->tilemap;
        u32 min_tile_x = (u32)(min_pos.x / (tilemap->tile_size.x));
        u32 max_tile_x = (u32)(max_pos.x / (tilemap->tile_size.x)) + 1;
        u32 min_tile_y = (u32)(min_pos.y / (tilemap->tile_size.y));
        u32 max_tile_y = (u32)(max_pos.y / (tilemap->tile_size.y)) + 1;

        f32 t_remaining = 1.0f; // Full movement has occurred at time = 1.0f.
        for (u32 collision_iter = 0; collision_iter < 4 && t_remaining > 0.0f; collision_iter++) {
            f32 t_min = 1.0f;
            v2 wall_normal = {0}; // Used to glide the player off the wall using his existing velocity.
            // NOTE(Wes): We use != to check against the max tile to get around integer overflow
            //            when reaching edge of the tilemap.
            for (u32 y = min_tile_y; y != max_tile_y; y++) {
                for (u32 x = min_tile_x; x != max_tile_x; x++) {
                    unsigned char *tile = get_tile(tilemap, x, y);
                    if (tile && !is_tile_empty(tile)) {

                        // Add the entity->size to the corners to take into account the entire size of the entity.
                        // This is a form of Minkowski Sum.
                        // TODO(Wes): Consider moving to a system where the entity position represents its center
                        // and not it's top left.
                        v2 min_corner = V2(x * tilemap->tile_size.x, y * tilemap->tile_size.y);
                        min_corner = v2_sub(min_corner, entity->size);
                        v2 max_corner = v2_add(min_corner, tilemap->tile_size);
                        max_corner = v2_add(max_corner, entity->size);

                        // Test left wall
                        if (test_wall(entity->position.x,
                                      entity->position.y,
                                      max_corner.x,
                                      min_corner.y,
                                      max_corner.y,
                                      pos_delta.x,
                                      pos_delta.y,
                                      &t_min)) {
                            wall_normal = V2(1.0f, 0.0f);
                        }
                        // Test right wall
                        if (test_wall(entity->position.x,
                                      entity->position.y,
                                      min_corner.x,
                                      min_corner.y,
                                      max_corner.y,
                                      pos_delta.x,
                                      pos_delta.y,
                                      &t_min)) {
                            wall_normal = V2(-1.0f, 0.0f);
                        }
                        // Test bottom wall
                        if (test_wall(entity->position.y,
                                      entity->position.x,
                                      max_corner.y,
                                      min_corner.x,
                                      max_corner.x,
                                      pos_delta.y,
                                      pos_delta.x,
                                      &t_min)) {
                            wall_normal = V2(0.0f, 1.0f);
                        }
                        // Test top wall
                        if (test_wall(entity->position.y,
                                      entity->position.x,
                                      min_corner.y,
                                      min_corner.x,
                                      max_corner.x,
                                      pos_delta.y,
                                      pos_delta.x,
                                      &t_min)) {
                            wall_normal = V2(0.0f, -1.0f);
                        }
                    }
                }
            }
            entity->position = v2_add(entity->position, v2_mul(pos_delta, t_min));

            // If we've hit a wall then wall_normal will be non-zero.
            entity->velocity = v2_sub(entity->velocity, v2_mul(wall_normal, v2_dot(entity->velocity, wall_normal)));
            pos_delta = v2_sub(pos_delta, v2_mul(wall_normal, v2_dot(pos_delta, wall_normal)));

            t_remaining -= t_remaining * t_min;
        }
    }
}

static image_t load_image(const char *path, load_file_fn load_file)
{
    // TODO(Wes): Add error checking and handle allocation on behalf of stb.
    // TODO(Wes): This should be hidden behind an abstraction exposed in the platform layer.
    image_t result = {0};
    loaded_file_t loaded_file = load_file(path);
    int unused = 0;
    int required_components = 4; // We always want RGBA.
    result.data = (u32 *)stbi_load_from_memory(
        loaded_file.contents, (int)loaded_file.size, (int *)&result.w, (int *)&result.h, &unused, required_components);

    // NOTE(Wes): Pre-multiply alpha.
    for (u32 y = 0; y < result.h; y++) {
        for (u32 x = 0; x < result.w; x++) {
            u32 *texel = &result.data[x + y * result.w];

            // Convert RGBA -> ARGB
            u32 temp = *texel;
            u32 a = (temp & 0xFF000000) >> 24;
            temp <<= 8;
            temp |= a;

            *texel = temp;

            v4 color = read_image_color(*texel);

            // NOTE(Wes): Convert to linear space before premultiplying alpha.
            color = srgb_to_linear(color);

            // NOTE(Wes): Premultiple Normalized alpha
            // but not normalized color values.
            v3 rgb = v3_mul(color.rgb, 255.0f);
            rgb = v3_mul(rgb, color.a);

            color.r = rgb.r / 255.0f;
            color.g = rgb.g / 255.0f;
            color.b = rgb.b / 255.0f;

            color = linear_to_srgb(color);
            *texel = color_image_32(color);
        }
    }
    return result;
}

game_memory_t *dbg_global_memory;
DLL_FN void game_update_and_render(game_memory_t *memory,
                                   game_frame_buffer_t *frame_buffer,
                                   game_audio_t *audio,
                                   game_input_t *input,
                                   game_output_t *output,
                                   game_callbacks_t *callbacks,
                                   game_work_queues_t *work_queues)
{
    dbg_global_memory = memory;
    START_COUNTER(game_update_and_render);

    assert(sizeof(game_state_t) <= memory->permanent_store_size);
    game_state_t *game_state = (game_state_t *)memory->permanent_store;
    if (!memory->is_initialized) {
        game_state->world.camera.units_to_pixels = 15.0f / 1.0f;
        game_state->world.camera.position.x = 0.0f;
        game_state->world.camera.position.y = 0.0f;
        game_state->world.gravity.x = 0.0f;
        game_state->world.gravity.y = 9.8f * 20;
        game_state->t_sine = 0.0f;
        game_state->tone_hz = 512;
        static unsigned char tilemap[18][32] = {
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        };
        unsigned char *tile = game_state->world.tilemap.tiles;
        for (u32 y = 0; y < 18; ++y) {
            for (u32 x = 0; x < 32; ++x) {
                *tile = tilemap[y][x];
                tile++;
            }
        }
        game_state->world.tilemap.tile_size.x = 4.0f;
        game_state->world.tilemap.tile_size.y = 4.0f;
        game_state->world.tilemap.tiles_wide = 32;
        game_state->world.tilemap.tiles_high = 18;

        game_state->background_image = load_image("data/background/Bg 1.png", callbacks->load_file);
        game_state->tile_image = load_image("data/tiles/Box 01.png", callbacks->load_file);
        game_state->player_image = load_image("data/player/test.png", callbacks->load_file);

        init_arena(&game_state->arena,
                   memory->permanent_store_size - sizeof(game_state_t),
                   memory->permanent_store + sizeof(game_state_t));
        init_arena(&game_state->frame_arena, memory->transient_store_size, memory->transient_store);

        // TODO(Wes): This breaks the hot reloading. Fix it.
        game_state->render_queue = render_alloc_queue(&game_state->frame_arena, 20000, &game_state->world.camera);

        memory->is_initialized = 1;
    }

#ifdef GG_EDITOR
    for (u32 i = 0; i < GG_MAX_CONTROLLERS; ++i) {
        game_controller_input_t *controller = &input->controllers[i];
        if (controller->editor_mode.ended_down) {
            game_state->editor_enabled = !game_state->editor_enabled;
        }
    }
#endif

    // NOTE(Wes): Check for controller based entity spawn.
    for (u32 i = 0; i < GG_MAX_CONTROLLERS; ++i) {
        game_controller_input_t *controller = &input->controllers[i];
        u32 controlled_entity = game_state->world.controlled_entities[i];
        if (controller->start.ended_down && !controlled_entity) {
            controlled_entity = get_next_entity(game_state->world.entities);
            game_state->world.controlled_entities[i] = controlled_entity;

            // NOTE(Wes): Initialize the player
            entity_t *entity = &game_state->world.entities[controlled_entity];
            entity->type = entity_type_player;
            entity->position.x = 80.0f;
            entity->position.y = 54.0f;
            entity->size.x = 2.0f;
            entity->size.y = 4.0f;
            entity->exists = 1;
            entity->velocity_factor = -7.0f; // Drag
            entity->acceleration_factor = 150.0f;
        }
    }

    update_entities(game_state, input, callbacks->log);

    // Update draw offset and units_to_pixels so that camera always sees all
    // players

    v2 min_pos = V2(FLT_MAX, FLT_MAX);
    v2 max_pos = V2(FLT_MIN, FLT_MIN);

    for (u32 i = 0; i < GG_MAX_CONTROLLERS; ++i) {
        u32 controlled_entity = game_state->world.controlled_entities[i];
        if (controlled_entity) {
            entity_t *entity = &game_state->world.entities[controlled_entity];
            if (entity->type == entity_type_player) {
                if (entity->position.x < min_pos.x) {
                    min_pos.x = entity->position.x;
                }
                if (entity->position.y < min_pos.y) {
                    min_pos.y = entity->position.y;
                }
                if (entity->position.x > max_pos.x) {
                    max_pos.x = entity->position.x;
                }
                if (entity->position.y > max_pos.y) {
                    max_pos.y = entity->position.y;
                }
            }
        }
    }

    // min_pos = V2(0, 0);
    tilemap_t *tilemap = &game_state->world.tilemap;
    // max_pos = V2(tilemap->tile_size.x * tilemap->tiles_wide, tilemap->tile_size.y * tilemap->tiles_high);

    v2 camera_edge_buffer = V2(10.0f, 10.0f);
    min_pos = v2_sub(min_pos, camera_edge_buffer);
    max_pos = v2_add(max_pos, camera_edge_buffer);
    v2 new_camera_pos = v2_div(v2_add(min_pos, max_pos), 2.0f);

    f32 cam_move_speed = 4.0f;
    game_state->world.camera.position =
        v2_add(v2_mul(game_state->world.camera.position, (1.0f - (input->delta_time * cam_move_speed))),
               v2_mul(new_camera_pos, (input->delta_time * cam_move_speed)));
    game_state->world.camera.position = new_camera_pos;

    // NOTE(Wes): Start by clearing the screen.
    render_push_clear(game_state->render_queue, COLOR(1.0f, 0.5f, 0.5f, 0.5f));
    // render_push_clear(game_state->render_queue, COLOR(1.0f, 1.0f, 0.0f, 0.0f));
    game_state->elapsed_time += input->delta_time;
    f32 angle = game_state->elapsed_time * 0.5f;
    v2 light_origin = v2_mul(V2(kcosf(angle), ksinf(angle)), 10);
    light_origin = v2_add(light_origin, V2(50.0f, 50.0f));
    v4 light_color = V4(1.0f, 1.0f, 1.0f, 1.0f);
    v3 light_pos = V3(light_origin.x, light_origin.y, 15.0f);
    v4 ambient = V4(1.0f, 1.0f, 1.0f, 1.0f);
    f32 radius = 100.0f;
    light_t light = {light_color, light_pos, ambient, radius};

#if 1
    v2 background_pos = V2(0.0f, 0.0f);
    v2 background_size = V2(128.0f, 72.0f);
    basis_t background_basis = {background_pos, V2(background_size.x, 0.0f), V2(0.0f, background_size.y)};
    render_push_image(game_state->render_queue,
                      &background_basis,
                      V4(1.0f, 1.0f, 1.0f, 1.0f),
                      &game_state->background_image,
                      0,
                      &light,
                      1);
    for (u32 i = 0; i < 18; ++i) {
        for (u32 j = 0; j < 32; ++j) {
            if (tilemap->tiles[j + i * tilemap->tiles_wide]) {
                v2 tile_origin;
                tile_origin.x = j * tilemap->tile_size.x;
                tile_origin.y = i * tilemap->tile_size.y;
                basis_t tile_basis = {tile_origin, V2(tilemap->tile_size.x, 0.0f), V2(0.0f, tilemap->tile_size.y)};
                render_push_image(game_state->render_queue,
                                  &tile_basis,
                                  V4(1.0f, 1.0f, 1.0f, 1.0f),
                                  &game_state->tile_image,
                                  0,
                                  &light,
                                  1);
            }
        }
    }
#endif

    for (u32 i = 0; i < GG_MAX_CONTROLLERS; ++i) {
        u32 controlled_entity = game_state->world.controlled_entities[i];
        if (controlled_entity == 0) {
            continue;
        }
        entity_t *entity = &game_state->world.entities[controlled_entity];
        if (!entity->exists) {
            continue;
        }

        if (entity->type == entity_type_player) {
            v2 draw_offset = V2(0.0f, 0.0f);
            v2 draw_pos = v2_add(entity->position, draw_offset);
            basis_t player_basis = {draw_pos, V2(entity->size.x, 0.0f), V2(0.0f, entity->size.y)};
            render_push_image(game_state->render_queue,
                              &player_basis,
                              V4(1.0f, 1.0f, 1.0f, 1.0f),
                              &game_state->player_image,
                              &game_state->player_normal,
                              &light,
                              1);
        }
    }

#if 1
    v2 o = {{60.0f, 26.0f}};
    v2 x = {{20.0f, 2.0f}};
    v2 y = v2_perp(x);
    v2 o2 = {{70.0f, 30.0f}};
    v2 x2 = {{10.0f, 0.0f}};
    v2 y2 = v2_perp(x2);
    basis_t line1_basis = {o, x, y};
    basis_t line2_basis = {o2, x2, y2};

    v4 line_color1 = COLOR(1.0f, 1.0f, 0.0f, 0.0f);
    v4 line_color2 = COLOR(1.0f, 0.0f, 1.0f, 0.0f);
    if (collider_contains_point(&line1_basis, line2_basis.origin)) {
        line_color2 = COLOR(1.0f, 0.0f, 1.0f, 0.0f);
    }

    render_push_rect(game_state->render_queue, &line1_basis, V2(0, 0), V2(0, 0), 5.0f, line_color1);
    render_push_rect(game_state->render_queue, &line2_basis, V2(0, 0), V2(0, 0), 5.0f, line_color2);
#endif
    render_draw_queue(game_state->render_queue, frame_buffer, work_queues);

    output_sine_wave(game_state, audio);

    END_COUNTER(game_update_and_render);
}
