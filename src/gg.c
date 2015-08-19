#include "gg_types.h"
#include "gg_math.h"
#include "gg_vec.h"
#include "gg_render.h"

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
    return &tilemap->tiles[x + y * tilemap->tiles_wide];
}

typedef struct {
    f32 intersect;
    b8 did_intersect;
} ray_cast_result;

static ray_cast_result ray_cast_vertical(v2 origin, f32 end_y, tilemap_t *tilemap)
{
    ray_cast_result result;
    f32 tile_width = tilemap->tile_size.x;
    f32 tile_height = tilemap->tile_size.y;
    i32 tilemap_start_y = origin.y / tile_height;
    i32 tilemap_x = origin.x / tile_width;
    i32 tilemap_end_y = end_y / tile_height;

    for (i32 i = tilemap_start_y; i <= tilemap_end_y; ++i) {
        if (*get_tile(tilemap, tilemap_x, i)) {
            result.intersect = i * tile_height;
            result.did_intersect = 1;
            return result;
        }
    }
    for (i32 i = tilemap_start_y - 1; i >= tilemap_end_y; --i) {
        if (*get_tile(tilemap, tilemap_x, i)) {
            result.intersect = (i + 1) * tile_height;
            result.did_intersect = 1;
            return result;
        }
    }
    // No intersect found if we get here so just set it to as far as
    // requested.
    result.intersect = end_y;
    result.did_intersect = 0;
    return result;
}

static ray_cast_result ray_cast_horizontal(v2 origin, f32 end_x, tilemap_t *tilemap)
{
    ray_cast_result result;
    f32 tile_width = tilemap->tile_size.x;
    f32 tile_height = tilemap->tile_size.y;
    i32 tilemap_start_x = origin.x / tile_width;
    i32 tilemap_y = origin.y / tile_height;
    i32 tilemap_end_x = end_x / tile_width;

    for (i32 i = tilemap_start_x; i <= tilemap_end_x; ++i) {
        unsigned char *tile = get_tile(tilemap, i, tilemap_y);
        if (*tile) {
            result.intersect = i * tile_width;
            result.did_intersect = 1;
            return result;
        }
    }
    for (i32 i = tilemap_start_x - 1; i >= tilemap_end_x; --i) {
        unsigned char *tile = get_tile(tilemap, i, tilemap_y);
        if (*tile) {
            result.intersect = (i + 1) * tile_width;
            result.did_intersect = 1;
            return result;
        }
    }
    // No intersect found if we get here so just set it to as far as
    // requested.
    result.intersect = end_x;
    result.did_intersect = 0;
    return result;
}

static u32 get_next_entity(entity_t *entities)
{
    static const entity_t zero_entity = {};
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
    static const entity_t zero_entity = {};
    assert(index < GG_MAX_ENTITIES);
    entities[index] = zero_entity;
}

static void update_entities(game_state_t *game_state, game_input_t *input)
{
    world_t *world = &game_state->world;
    v2 new_accels[GG_MAX_ENTITIES] = {};

    for (u32 i = 0; i < GG_MAX_CONTROLLERS; ++i) {
        u32 entity_index = world->controlled_entities[i];
        if (entity_index != 0) {
            entity_t *entity = &world->entities[entity_index];
            if (!entity->exists) {
            }
            game_controller_input_t *controller = &input->controllers[i];

            // Jump
            if (controller->action_down.ended_down && entity->on_ground) {
                entity->velocity.y = -80.0f;
            }

            // Throw teleporter
            if (controller->right_shoulder.ended_down && entity->type == entity_type_player &&
                !entity->player.teleporter_index) {
                entity->player.teleporter_index = get_next_entity(world->entities);
                u32 teleporter_index = entity->player.teleporter_index;
                if (teleporter_index != 0) {
                    entity_t *teleporter_entity = &world->entities[teleporter_index];
                    teleporter_entity->type = entity_type_teleporter;
                    teleporter_entity->position = entity->position;
                    teleporter_entity->velocity = entity->velocity;
                    teleporter_entity->size.x = 2.0f;
                    teleporter_entity->size.y = 2.0f;
                    teleporter_entity->velocity_factor = -2.0f;
                    teleporter_entity->acceleration_factor = 100.0f;
                    teleporter_entity->teleporter.image = &game_state->green_teleporter;
                    v2 left_stick;
                    left_stick.x = controller->left_stick_x;
                    left_stick.y = controller->left_stick_y;
                    teleporter_entity->velocity = v2_mul(left_stick, teleporter_entity->acceleration_factor);
                    // f32 acceleration_factor = 150.0f;
                    // new_accels[entity->teleporter_index]
                    // =
                    //   v2_mul(left_stick,
                    //   acceleration_factor);
                }
            }

            // Attack
            if (controller->action_right.ended_down && entity->type == entity_type_player &&
                !entity->player.attacking) {
                entity->player.attacking = 1;
            }

            // Teleport
            if (controller->left_shoulder.ended_down && entity->type == entity_type_player &&
                entity->player.teleporter_index) {
                entity_t *teleporter_entity = &world->entities[entity->player.teleporter_index];
                entity->position = teleporter_entity->position;
                free_entity(world->entities, entity->player.teleporter_index);
                entity->player.teleporter_index = 0;
            }

            // Move
            v2 left_stick = {};
            if (controller->is_analog) {
                left_stick.x = controller->left_stick_x;
            } else {
                if (controller->move_left.ended_down) {
                    left_stick.x = -1.0f;
                }
                if (controller->move_right.ended_down) {
                    left_stick.x = 1.0f;
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
        f32 collision_buffer = 0.001f;
        v2 new_accel = new_accels[i];

        v2 half_entity_size = v2_div(entity->size, 2.0f);

        // Gravity
        new_accel = v2_add(new_accel, world->gravity);

        // NOTE(Wes): We always ray cast 4 times for movement:
        // If we are moving left we cast left from the left side at the
        // top and bottom of the entity, vice versa
        // for
        // right.
        // If we are moving down we cast down from the bottom side at
        // the left and right of the entity, vice
        // versa for
        // up.

        // Friction force. USE ODE here!
        v2 friction = V2(entity->velocity_factor * entity->velocity.x, 0.0f);
        new_accel = v2_add(new_accel, friction);

        // Velocity verlet integration.
        v2 average_accel = v2_div(v2_add(entity->acceleration, new_accel), 2);
        v2 new_entity_pos = v2_mul(v2_mul(entity->acceleration, 0.5f), input->delta_time * input->delta_time);
        new_entity_pos = v2_add(v2_mul(entity->velocity, input->delta_time), new_entity_pos);
        new_entity_pos = v2_add(entity->position, new_entity_pos);
        entity->velocity = v2_add(v2_mul(average_accel, input->delta_time), entity->velocity);
        entity->acceleration = new_accel;

        // Get the movement direction as -1 = left/up, 0 = unused, 1 =
        // right/down
        v2i move_dir = v2_sign(entity->velocity);

        // Horizontal collision check.
        f32 cast_x = half_entity_size.x * move_dir.x;
        v2 top_origin;
        top_origin.x = entity->position.x + cast_x;
        top_origin.y = entity->position.y - half_entity_size.y + collision_buffer;
        v2 bot_origin;
        bot_origin.x = top_origin.x;
        bot_origin.y = entity->position.y + half_entity_size.y - collision_buffer;

        f32 end_x = new_entity_pos.x + cast_x;
        ray_cast_result intersect_x1 = ray_cast_horizontal(top_origin, end_x, &world->tilemap);
        ray_cast_result intersect_x2 = ray_cast_horizontal(bot_origin, end_x, &world->tilemap);

        if ((move_dir.x * intersect_x1.intersect) <= (move_dir.x * intersect_x2.intersect)) {
            entity->position.x = intersect_x1.intersect - cast_x;
        } else {
            entity->position.x = intersect_x2.intersect - cast_x;
        }

        if (intersect_x1.did_intersect || intersect_x2.did_intersect) {
            entity->velocity.x = 0.0f;
            entity->acceleration.x = 0.0f;
        }

        // Vertical collision check.
        f32 cast_y = half_entity_size.y * move_dir.y;
        v2 left_origin;
        left_origin.x = entity->position.x - half_entity_size.x + collision_buffer;
        left_origin.y = entity->position.y + cast_y;
        v2 right_origin;
        right_origin.x = entity->position.x + half_entity_size.x - collision_buffer;
        right_origin.y = left_origin.y;

        f32 end_y = new_entity_pos.y + cast_y;
        ray_cast_result intersect_y1 = ray_cast_vertical(left_origin, end_y, &world->tilemap);
        ray_cast_result intersect_y2 = ray_cast_vertical(right_origin, end_y, &world->tilemap);

        if ((move_dir.y * intersect_y1.intersect) <= (move_dir.y * intersect_y2.intersect)) {
            entity->position.y = intersect_y1.intersect - cast_y;
        } else {
            entity->position.y = intersect_y2.intersect - cast_y;
        }

        if (intersect_y1.did_intersect || intersect_y2.did_intersect) {
            entity->velocity.y = 0.0f;
            entity->acceleration.y = 0.0f;
            if (move_dir.y == 1) {
                entity->on_ground = 1;
            } else {
                entity->on_ground = 0;
            }
        } else {
            entity->on_ground = 0;
        }

        // Track entity positions for the camera
        if (entity->type == entity_type_player || entity->type == entity_type_teleporter) {
            game_state->world.camera_tracked_positions[i] = entity->position;
        }

        // If this entity is a sword then check if it is currently
        // intersecting with any players
        if (entity->type == entity_type_player && entity->player.attacking) {
            for (u32 j = 1; j < GG_MAX_ENTITIES; ++j) {
                entity_t *other_player = &world->entities[j];
                if (i == j || !other_player->exists || other_player->type != entity_type_player) {
                    continue;
                }
                v2 katana_pos;
                if (entity->velocity.x > 0) {
                    katana_pos = v2_add(entity->position, entity->player.katana_offset);
                } else {
                    katana_pos = v2_sub(entity->position, entity->player.katana_offset);
                }
                v2 player_pos = other_player->position;
                v2 player_half_size = v2_div(other_player->size, 2.0f);
                if (player_pos.x - player_half_size.x < katana_pos.x &&
                    player_pos.x + player_half_size.x > katana_pos.x &&
                    player_pos.y - player_half_size.y < katana_pos.y &&
                    player_pos.y + player_half_size.y > katana_pos.y) {
                    for (u32 k = 0; k < GG_MAX_CONTROLLERS; ++k) {
                        if (world->controlled_entities[k] == j) {
                            world->controlled_entities[k] = 0;
                        }
                    }

                    other_player->exists = 0;
                    if (other_player->player.teleporter_index) {
                        entity_t *other_teleporter = &world->entities[other_player->player.teleporter_index];
                        if (other_teleporter->exists) {
                            other_teleporter->exists = 0;
                            other_teleporter->player.teleporter_index = 0;
                        }
                    }
                }
            }
        }
    }
}

static image_t load_image(const char *path, map_file_fn map_file)
{
    // TODO(Wes): Add error checking and handle allocation on behalf of stb.
    image_t result = {};
    mapped_file_t mapped_file = map_file(path);
    int unused = 0;
    int required_components = 4; // We always want RGBA.
    result.data = (u32 *)stbi_load_from_memory(
        mapped_file.contents, mapped_file.size, (int *)&result.w, (int *)&result.h, &unused, required_components);

    // NOTE(Wes): Pre-multiply alpha.
    for (u32 y = 0; y < result.h; y++) {
        for (u32 x = 0; x < result.w; x++) {
            u32 *texel = &result.data[x + y * result.w];
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

void game_update_and_render(game_memory_t *memory,
                            game_frame_buffer_t *frame_buffer,
                            game_audio_t *audio,
                            game_input_t *input,
                            game_output_t *output,
                            game_callbacks_t *callbacks)
{
    if (!memory) {
        return;
    }

    assert(sizeof(game_state_t) <= memory->permanent_store_size);
    game_state_t *game_state = (game_state_t *)memory->permanent_store;
    if (!memory->is_initialized) {
        game_state->world.camera.units_to_pixels = 10.0f / 1.0f;
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

        game_state->test_image = load_image("data/test_image.png", callbacks->map_file);
        game_state->test_image2 = load_image("data/test_image2.png", callbacks->map_file);
        game_state->test_image3 = load_image("data/test_image3.png", callbacks->map_file);
        game_state->background_image = load_image("data/background/Bg 1.png", callbacks->map_file);
        game_state->tile_image = load_image("data/tiles/Box 01.png", callbacks->map_file);
        // game_state->player_images[0] = load_image("data/player/walk_with_sword/1.png", callbacks->map_file);
        game_state->player_images[0] = load_image("data/sphere_normals.png", callbacks->map_file);
        game_state->player_images[1] = load_image("data/player/walk_with_sword/2.png", callbacks->map_file);
        game_state->player_images[2] = load_image("data/player/walk_with_sword/3.png", callbacks->map_file);
        game_state->player_images[3] = load_image("data/player/walk_with_sword/4.png", callbacks->map_file);
        game_state->player_images[4] = load_image("data/player/walk_with_sword/5.png", callbacks->map_file);
        game_state->player_images[5] = load_image("data/player/walk_with_sword/6.png", callbacks->map_file);
        game_state->player_normal = load_image("data/sphere_normals.png", callbacks->map_file);
        game_state->player_attack_images[0] = load_image("data/player/attack_with_sword/1.png", callbacks->map_file);
        game_state->player_attack_images[1] = load_image("data/player/attack_with_sword/2.png", callbacks->map_file);
        game_state->player_attack_images[2] = load_image("data/player/attack_with_sword/3.png", callbacks->map_file);
        game_state->player_attack_images[3] = load_image("data/player/attack_with_sword/4.png", callbacks->map_file);
        game_state->player_attack_images[4] = load_image("data/player/attack_with_sword/5.png", callbacks->map_file);
        game_state->player_attack_images[5] = load_image("data/player/attack_with_sword/6.png", callbacks->map_file);
        game_state->green_teleporter = load_image("data/teleporter/green.png", callbacks->map_file);

        init_arena(&game_state->arena,
                   memory->permanent_store_size - sizeof(game_state_t),
                   memory->permanent_store + sizeof(game_state_t));
        init_arena(&game_state->frame_arena, memory->transient_store_size, memory->transient_store);

        // TODO(Wes): This breaks the hot reloading. Fix it.
        game_state->render_queue = render_alloc_queue(&game_state->frame_arena, 10000, &game_state->world.camera);

        memory->is_initialized = 1;
    }

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
            entity->position.x = 20.0f;
            entity->position.y = 20.0f;
            entity->size.x = 2.0f;
            entity->size.y = 4.0f;
            entity->exists = 1;
            entity->velocity_factor = -7.0f; // Drag
            entity->acceleration_factor = 150.0f;
            entity->player.walk.frames = game_state->player_images;
            entity->player.walk.max_frames = 6;
            entity->player.walk.fps = 24.0f;
            entity->player.attack.frames = game_state->player_attack_images;
            entity->player.attack.max_frames = 6;
            entity->player.attack.fps = 24.0f;
            entity->player.katana_offset.x = 3.0f;
        }
    }

    update_entities(game_state, input);

    // Update draw offset and units_to_pixels so that camera always sees all
    // players

    v2 min_pos = V2(FLT_MAX, FLT_MAX);
    v2 max_pos = V2(FLT_MIN, FLT_MIN);
    b8 tracked_pos_found = 0;
    for (u32 i = 0; i < GG_MAX_ENTITIES; ++i) {
        v2 tracked_pos = game_state->world.camera_tracked_positions[i];
        if (tracked_pos.x != 0.0f && tracked_pos.y != 0.0f) {
            tracked_pos_found = 1;
            if (tracked_pos.x < min_pos.x) {
                min_pos.x = tracked_pos.x;
            }
            if (tracked_pos.y < min_pos.y) {
                min_pos.y = tracked_pos.y;
            }
            if (tracked_pos.x > max_pos.x) {
                max_pos.x = tracked_pos.x;
            }
            if (tracked_pos.y > max_pos.y) {
                max_pos.y = tracked_pos.y;
            }
        }
    }
    // TODO(Wes): Override camera tracking and just watch the whole tilemap...
    // for now.
    // This should be removed sometime.
    min_pos = V2(0, 0);
    tilemap_t *tilemap = &game_state->world.tilemap;
    max_pos = V2(tilemap->tile_size.x * tilemap->tiles_wide, tilemap->tile_size.y * tilemap->tiles_high);
    memset(game_state->world.camera_tracked_positions, 0, GG_MAX_ENTITIES * sizeof(v2));

    v2 camera_edge_buffer = V2(0.0f, 0.0f);
    min_pos = v2_sub(min_pos, camera_edge_buffer);
    max_pos = v2_add(max_pos, camera_edge_buffer);
    v2 new_camera_pos = v2_div(v2_add(min_pos, max_pos), 2.0f);

    f32 cam_move_speed = 4.0f;
    game_state->world.camera.position =
        v2_add(v2_mul(game_state->world.camera.position, (1.0f - (input->delta_time * cam_move_speed))),
               v2_mul(new_camera_pos, (input->delta_time * cam_move_speed)));

    v2 screen_span = v2_sub(v2_add(max_pos, camera_edge_buffer), min_pos);
    f32 x_units_to_pixels = frame_buffer->w / screen_span.x;
    f32 y_units_to_pixels = frame_buffer->h / screen_span.y;

    f32 units_to_pixels = x_units_to_pixels < y_units_to_pixels ? x_units_to_pixels : y_units_to_pixels;
    game_state->world.camera.units_to_pixels = units_to_pixels;

    // TODO(Wes): Fix camera zoom rounding errors.
    f32 cam_zoom_speed = 0.0f;
    game_state->world.camera.units_to_pixels =
        (game_state->world.camera.units_to_pixels * (1.0f - (input->delta_time * cam_zoom_speed))) +
        (units_to_pixels * (input->delta_time * cam_zoom_speed));

#if 0
    // Clamp draw offset.
    f32 units_to_pixels = x_units_to_pixels < y_units_to_pixels
                              ? x_units_to_pixels
                              : y_units_to_pixels;
    if (game_state->world.camera.position.x < 0.0f) {
        game_state->world.camera.position.x = 0.0;
    } else if (game_state->world.camera.position.x +
                   frame_buffer->width / units_to_pixels >=
               32 * game_state->world.tilemap.tile_size.x) {
        game_state->world.camera.position.x =
            (32 * game_state->world.tilemap.tile_size.x) -
            frame_buffer->width / units_to_pixels;
    }
    if (game_state->world.camera.position.y < 0.0f) {
        game_state->world.camera.position.y = 0.0;
    } else if (game_state->world.camera.position.y +
                   frame_buffer->height / units_to_pixels >=
               18 * game_state->world.tilemap.tile_size.y) {
        game_state->world.camera.position.y =
            (18 * game_state->world.tilemap.tile_size.y) -
            frame_buffer->height / units_to_pixels;
    }
#endif
    // NOTE(Wes): Start by clearing the screen.
    render_push_clear(game_state->render_queue, COLOR(1.0f, 1.0f, 1.0f, 1.0f));

#if 1
    v2 background_pos = V2(64.0f, 36.0f);
    v2 background_size = V2(128.0f, 72.0f);
    render_push_image(game_state->render_queue, background_pos, background_size, &game_state->background_image, 0);
    for (u32 i = 0; i < 18; ++i) {
        for (u32 j = 0; j < 32; ++j) {
            if (tilemap->tiles[j + i * tilemap->tiles_wide]) {
                v2 tile_origin;
                tile_origin.x = j * tilemap->tile_size.x;
                tile_origin.y = i * tilemap->tile_size.y;
                v2 tile_half_size = v2_div(tilemap->tile_size, 2.0f);
                tile_origin = v2_add(tile_origin, tile_half_size);
                render_push_image(
                    game_state->render_queue, tile_origin, tilemap->tile_size, &game_state->tile_image, 1);
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
            entity_anim_t *anim;
            if (entity->player.attacking) {
                anim = &entity->player.attack;
                if (anim->accumulator + input->delta_time >= (1.0f / anim->fps)) {
                    anim->current_frame = ++anim->current_frame % anim->max_frames;
                    anim->accumulator = 0;
                    // TODO(Wes): This is not a good way to
                    // determine when attack is complete.
                    if (anim->current_frame == 0) {
                        entity->player.attacking = 0;
                    }
                } else {
                    anim->accumulator += input->delta_time;
                }
            } else {
                anim = &entity->player.walk;
                if (input->controllers[i].left_stick_x != 0.0f) {
                    f32 anim_fps = kabsf(input->controllers[i].left_stick_x) * anim->fps;
                    if (anim->accumulator + input->delta_time >= (1.0f / anim_fps)) {
                        anim->current_frame = ++anim->current_frame % anim->max_frames;
                        anim->accumulator = 0;
                    } else {
                        anim->accumulator += input->delta_time;
                    }
                }
            }
            v2 draw_offset = V2(0.3f, -0.4f);
            v2 size = V2(8.0f, 5.0f);
            v2 draw_pos = v2_add(entity->position, draw_offset);
            render_push_image(
                game_state->render_queue, draw_pos, size, &anim->frames[anim->current_frame], entity->velocity.x > 0);

            // Debug drawing for katana point.
            if (entity->player.attacking) {
                v2 katana_pos;
                if (entity->velocity.x > 0) {
                    katana_pos = v2_add(entity->position, entity->player.katana_offset);
                } else {
                    katana_pos = v2_sub(entity->position, entity->player.katana_offset);
                }
                v2 size = V2(1.0f, 1.0f);
                render_push_block(game_state->render_queue, katana_pos, size, COLOR(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }

        if (entity->type == entity_type_player && entity->player.teleporter_index) {
            entity_t *teleporter = &game_state->world.entities[entity->player.teleporter_index];
            assert(teleporter->type == entity_type_teleporter);
            render_push_image(
                game_state->render_queue, teleporter->position, teleporter->size, teleporter->teleporter.image, 0);
        }
    }

    game_state->elapsed_time += input->delta_time;
    f32 angle = game_state->elapsed_time * 0.5f;
// f32 disp = kcosf(angle) * 50.0f;
#if 0
    v2 x_axis = v2_mul(V2(kcosf(angle), ksinf(angle)), 60);
    v2 y_axis = v2_perp(x_axis);
#else
    v2 x_axis = V2(20.0f, 0.0f);
    v2 y_axis = V2(0.0f, 20.0f);
#endif

#if 0
    v2 origin = V2(40.0f, 40.0f);
#else
    v2 origin = v2_mul(V2(kcosf(angle), ksinf(angle)), 10);
    origin = v2_add(origin, V2(50.0f, 50.0f));
#endif
    // origin = v2_add(origin, V2(disp, 0.0f));
    // origin = v2_sub(origin, v2_mul(x_axis, 0.5f));
    // origin = v2_sub(origin, v2_mul(y_axis, 0.5f));
    render_basis_t basis = {origin, x_axis, y_axis};
    v4 light_color = V4(1.0f, 1.0f, 1.0f, 1.0f);
    v3 light_pos = V3(60.0f, 60.0f, 5.0f);
    v4 ambient = V4(0.0f, 0.0f, 0.0f, 0.2f);
    f32 constant_attenuation = 1.0f;
    f32 linear_attenuation = 0.0045f;
    f32 quadratic_attenuation = 0.0f;
    light_t light = {light_color, light_pos, ambient, constant_attenuation, linear_attenuation, quadratic_attenuation};
    render_push_rotated_block(game_state->render_queue,
                              &basis,
                              V2(2, 2),
                              COLOR(1, 1, 1, 1),
                              &game_state->player_images[0],
                              &game_state->player_normal,
                              &light,
                              1);

    render_draw_queue(game_state->render_queue, frame_buffer);

    output_sine_wave(game_state, audio);
}
