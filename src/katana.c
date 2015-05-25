#include "katana_types.h"
#include "katana_intrinsics.h"
#include "katana_math.h"
#include "katana_vec.c"

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

static inline void linear_blend(u32 src, u32 *dest)
{
        u8 dest_b = (*dest >> 16) & 0xFF;
        u8 dest_g = (*dest >> 8) & 0xFF;
        u8 dest_r = (*dest >> 0) & 0xFF;

        u8 src_a = (src >> 24) & 0xFF;
        u8 src_b = (src >> 16) & 0xFF;
        u8 src_g = (src >> 8) & 0xFF;
        u8 src_r = (src >> 0) & 0xFF;

        u8 b = ((src_b * src_a) + (dest_b * (255 - src_a))) >> 8;
        u8 g = ((src_g * src_a) + (dest_g * (255 - src_a))) >> 8;
        u8 r = ((src_r * src_a) + (dest_r * (255 - src_a))) >> 8;

        *dest = (u32)b << 16 | (u32)g << 8 | (u32)r;
}

static void draw_image(vec2f_t pos, vec2f_t size, camera_t *camera, image_t *image, game_frame_buffer_t *frame_buffer,
                       b8 flip_x)
{
        assert(image);

        f32 units_to_pixels = camera->units_to_pixels;
        f32 frame_width_units = frame_buffer->width / units_to_pixels;
        f32 frame_height_units = frame_buffer->height / units_to_pixels;
        vec2f_t frame_buffer_half_size = {frame_width_units / 2.0f, frame_height_units / 2.0f};

        // Resizing ratio.
        vec2f_t actual_size;
        actual_size.x = image->width / units_to_pixels;
        actual_size.y = image->height / units_to_pixels;
        f32 x_ratio = actual_size.x / size.x;
        f32 y_ratio = actual_size.y / size.y;

        // Calculate top left and bottom right of block.
        vec2f_t half_size = vec2f_div(size, 2.0f);
        vec2f_t top_left_corner = vec2f_sub(pos, half_size);
        vec2f_t bot_right_corner = vec2f_add(pos, half_size);

        // Add draw offsets.
        vec2f_t draw_offset = vec2f_sub(camera->position, frame_buffer_half_size);
        top_left_corner = vec2f_sub(top_left_corner, draw_offset);
        bot_right_corner = vec2f_sub(bot_right_corner, draw_offset);

        // Convert to pixel values and round to nearest integer.
        top_left_corner = vec2f_mul(top_left_corner, units_to_pixels);
        bot_right_corner = vec2f_mul(bot_right_corner, units_to_pixels);
        vec2i_t top_left_pixel;
        vec2i_t bot_right_pixel;
        vec2f_floor2(top_left_corner, bot_right_corner, &top_left_pixel, &bot_right_pixel);

        // Bounds checking
        i32 image_data_offset_x = -top_left_pixel.x;
        i32 image_data_offset_y = -top_left_pixel.y;
        i32 flipped_image_data_offset_x = bot_right_pixel.x;
        if (top_left_pixel.x < 0) {
                top_left_pixel.x = 0;
        } else if (top_left_pixel.x > (i32)frame_buffer->width) {
                top_left_pixel.x = frame_buffer->width;
        }
        if (top_left_pixel.y < 0) {
                top_left_pixel.y = 0;
        } else if (top_left_pixel.y > (i32)frame_buffer->height) {
                top_left_pixel.y = frame_buffer->height;
        }
        if (bot_right_pixel.x < 0) {
                bot_right_pixel.x = 0;
        } else if (bot_right_pixel.x > (i32)frame_buffer->width) {
                bot_right_pixel.x = frame_buffer->width;
        }
        if (bot_right_pixel.y < 0) {
                bot_right_pixel.y = 0;
        } else if (bot_right_pixel.y > (i32)frame_buffer->height) {
                bot_right_pixel.y = frame_buffer->height;
        }
        u32 *image_data = (u32 *)image->data;
        if (flip_x) {
                for (i32 i = top_left_pixel.y; i < bot_right_pixel.y; ++i) {
                        u32 sample_y = (i + image_data_offset_y) * y_ratio;
                        for (i32 j = top_left_pixel.x; j < bot_right_pixel.x; ++j) {
                                u32 *dest = &frame_buffer->pixels[j + i * frame_buffer->width];
                                u32 sample_x = (flipped_image_data_offset_x - j) * x_ratio;
                                linear_blend(image_data[sample_x + sample_y * image->width], dest);
                        }
                }
        } else {
                for (i32 i = top_left_pixel.y; i < bot_right_pixel.y; ++i) {
                        u32 sample_y = (i + image_data_offset_y) * y_ratio;
                        for (i32 j = top_left_pixel.x; j < bot_right_pixel.x; ++j) {
                                u32 *dest = &frame_buffer->pixels[j + i * frame_buffer->width];
                                u32 sample_x = (j + image_data_offset_x) * x_ratio;
                                linear_blend(image_data[sample_x + sample_y * image->width], dest);
                        }
                }
        }
}

static void draw_block(vec2f_t pos, vec2f_t size, camera_t *camera, game_frame_buffer_t *frame_buffer, u32 color)
{
        f32 units_to_pixels = camera->units_to_pixels;
        f32 frame_width_units = frame_buffer->width / units_to_pixels;
        f32 frame_height_units = frame_buffer->height / units_to_pixels;
        vec2f_t frame_buffer_half_size = {frame_width_units / 2.0f, frame_height_units / 2.0f};

        // Calculate top left and bottom right of block.
        vec2f_t half_size = vec2f_div(size, 2.0f);
        vec2f_t top_left_corner = vec2f_sub(pos, half_size);
        vec2f_t bot_right_corner = vec2f_add(pos, half_size);

        // Add draw offsets.
        vec2f_t draw_offset = vec2f_sub(camera->position, frame_buffer_half_size);
        top_left_corner = vec2f_sub(top_left_corner, draw_offset);
        bot_right_corner = vec2f_sub(bot_right_corner, draw_offset);

        // Convert to pixel values and round to nearest integer.
        top_left_corner = vec2f_mul(top_left_corner, units_to_pixels);
        bot_right_corner = vec2f_mul(bot_right_corner, units_to_pixels);
        vec2i_t top_left_pixel;
        vec2i_t bot_right_pixel;
        vec2f_floor2(top_left_corner, bot_right_corner, &top_left_pixel, &bot_right_pixel);

        // Bounds checking
        if (top_left_pixel.x < 0) {
                top_left_pixel.x = 0;
        } else if (top_left_pixel.x > (i32)frame_buffer->width) {
                top_left_pixel.x = frame_buffer->width;
        }
        if (top_left_pixel.y < 0) {
                top_left_pixel.y = 0;
        } else if (top_left_pixel.y > (i32)frame_buffer->height) {
                top_left_pixel.y = frame_buffer->height;
        }
        if (bot_right_pixel.x < 0) {
                bot_right_pixel.x = 0;
        } else if (bot_right_pixel.x > (i32)frame_buffer->width) {
                bot_right_pixel.x = frame_buffer->width;
        }
        if (bot_right_pixel.y < 0) {
                bot_right_pixel.y = 0;
        } else if (bot_right_pixel.y > (i32)frame_buffer->height) {
                bot_right_pixel.y = frame_buffer->height;
        }
        for (i32 i = top_left_pixel.y; i < bot_right_pixel.y; ++i) {
                for (i32 j = top_left_pixel.x; j < bot_right_pixel.x; ++j) {
                        u32 *dest = &frame_buffer->pixels[j + i * frame_buffer->width];
                        *dest = color;
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

static ray_cast_result ray_cast_vertical(vec2f_t origin, f32 end_y, tilemap_t *tilemap)
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

static ray_cast_result ray_cast_horizontal(vec2f_t origin, f32 end_x, tilemap_t *tilemap)
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
        for (u32 i = 1; i < KATANA_MAX_ENTITIES; ++i) {
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
        assert(index < KATANA_MAX_ENTITIES);
        entities[index] = zero_entity;
}

static void update_entities(game_state_t *game_state, game_input_t *input)
{
        world_t *world = &game_state->world;
        vec2f_t new_accels[KATANA_MAX_ENTITIES] = {};

        for (u32 i = 0; i < KATANA_MAX_CONTROLLERS; ++i) {
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
                                        vec2f_t left_stick;
                                        left_stick.x = controller->left_stick_x;
                                        left_stick.y = controller->left_stick_y;
                                        teleporter_entity->velocity =
                                            vec2f_mul(left_stick, teleporter_entity->acceleration_factor);
                                        // f32 acceleration_factor = 150.0f;
                                        // new_accels[entity->teleporter_index]
                                        // =
                                        //   vec2f_mul(left_stick,
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
                        vec2f_t left_stick = {};
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
                        new_accels[entity_index] = vec2f_mul(left_stick, entity->acceleration_factor);
                }
        }

        for (u32 i = 1; i < KATANA_MAX_ENTITIES; ++i) {
                entity_t *entity = &world->entities[i];
                if (!entity->exists) {
                        continue;
                }

                // Ensures we never move exactly onto the tile we are colliding
                // with.
                f32 collision_buffer = 0.001f;
                vec2f_t new_accel = new_accels[i];

                vec2f_t half_entity_size = vec2f_div(entity->size, 2.0f);

                // Gravity
                new_accel = vec2f_add(new_accel, world->gravity);

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
                vec2f_t friction = vec2f(entity->velocity_factor * entity->velocity.x, 0.0f);
                new_accel = vec2f_add(new_accel, friction);

                // Velocity verlet integration.
                vec2f_t average_accel = vec2f_div(vec2f_add(entity->acceleration, new_accel), 2);
                vec2f_t new_entity_pos =
                    vec2f_mul(vec2f_mul(entity->acceleration, 0.5f), input->delta_time * input->delta_time);
                new_entity_pos = vec2f_add(vec2f_mul(entity->velocity, input->delta_time), new_entity_pos);
                new_entity_pos = vec2f_add(entity->position, new_entity_pos);
                entity->velocity = vec2f_add(vec2f_mul(average_accel, input->delta_time), entity->velocity);
                entity->acceleration = new_accel;

                // Get the movement direction as -1 = left/up, 0 = unused, 1 =
                // right/down
                vec2i_t move_dir = vec2f_sign(entity->velocity);

                // Horizontal collision check.
                f32 cast_x = half_entity_size.x * move_dir.x;
                vec2f_t top_origin;
                top_origin.x = entity->position.x + cast_x;
                top_origin.y = entity->position.y - half_entity_size.y + collision_buffer;
                vec2f_t bot_origin;
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
                vec2f_t left_origin;
                left_origin.x = entity->position.x - half_entity_size.x + collision_buffer;
                left_origin.y = entity->position.y + cast_y;
                vec2f_t right_origin;
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
                        for (u32 j = 1; j < KATANA_MAX_ENTITIES; ++j) {
                                entity_t *other_player = &world->entities[j];
                                if (i == j || !other_player->exists || other_player->type != entity_type_player) {
                                        continue;
                                }
                                vec2f_t katana_pos;
                                if (entity->velocity.x > 0) {
                                        katana_pos = vec2f_add(entity->position, entity->player.katana_offset);
                                } else {
                                        katana_pos = vec2f_sub(entity->position, entity->player.katana_offset);
                                }
                                vec2f_t player_pos = other_player->position;
                                vec2f_t player_half_size = vec2f_div(other_player->size, 2.0f);
                                if (player_pos.x - player_half_size.x < katana_pos.x &&
                                    player_pos.x + player_half_size.x > katana_pos.x &&
                                    player_pos.y - player_half_size.y < katana_pos.y &&
                                    player_pos.y + player_half_size.y > katana_pos.y) {
                                        for (u32 k = 0; k < KATANA_MAX_CONTROLLERS; ++k) {
                                                if (world->controlled_entities[k] == j) {
                                                        world->controlled_entities[k] = 0;
                                                }
                                        }

                                        other_player->exists = 0;
                                        if (other_player->player.teleporter_index) {
                                                entity_t *other_teleporter =
                                                    &world->entities[other_player->player.teleporter_index];
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
        int components = 0;
        int required_components = 4; // We always want RGBA.
        result.data = stbi_load_from_memory(mapped_file.contents, mapped_file.size, &result.width, &result.height,
                                            &components, required_components);
        return result;
}

void game_update_and_render(game_memory_t *memory, game_frame_buffer_t *frame_buffer, game_audio_t *audio,
                            game_input_t *input, game_output_t *output, game_callbacks_t *callbacks)
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

                game_state->background_image = load_image("data/background/Bg 1.png", callbacks->map_file);
                game_state->tile_image = load_image("data/tiles/Box 01.png", callbacks->map_file);
                game_state->player_images[0] = load_image("data/player/walk_with_sword/1.png", callbacks->map_file);
                game_state->player_images[1] = load_image("data/player/walk_with_sword/2.png", callbacks->map_file);
                game_state->player_images[2] = load_image("data/player/walk_with_sword/3.png", callbacks->map_file);
                game_state->player_images[3] = load_image("data/player/walk_with_sword/4.png", callbacks->map_file);
                game_state->player_images[4] = load_image("data/player/walk_with_sword/5.png", callbacks->map_file);
                game_state->player_images[5] = load_image("data/player/walk_with_sword/6.png", callbacks->map_file);
                game_state->player_attack_images[0] =
                    load_image("data/player/attack_with_sword/1.png", callbacks->map_file);
                game_state->player_attack_images[1] =
                    load_image("data/player/attack_with_sword/2.png", callbacks->map_file);
                game_state->player_attack_images[2] =
                    load_image("data/player/attack_with_sword/3.png", callbacks->map_file);
                game_state->player_attack_images[3] =
                    load_image("data/player/attack_with_sword/4.png", callbacks->map_file);
                game_state->player_attack_images[4] =
                    load_image("data/player/attack_with_sword/5.png", callbacks->map_file);
                game_state->player_attack_images[5] =
                    load_image("data/player/attack_with_sword/6.png", callbacks->map_file);
                game_state->green_teleporter = load_image("data/teleporter/green.png", callbacks->map_file);

                init_arena(&game_state->arena, memory->permanent_store_size - sizeof(game_state_t),
                           memory->permanent_store + sizeof(game_state_t));

                memory->is_initialized = 1;
        }

        // NOTE(Wes): Check for controller based entity spawn.
        for (u32 i = 0; i < KATANA_MAX_CONTROLLERS; ++i) {
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
        vec2f_t min_pos = {FLT_MAX, FLT_MAX};
        vec2f_t max_pos = {FLT_MIN, FLT_MIN};
        b8 tracked_pos_found = 0;
        for (u32 i = 0; i < KATANA_MAX_ENTITIES; ++i) {
                vec2f_t tracked_pos = game_state->world.camera_tracked_positions[i];
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
        memset(game_state->world.camera_tracked_positions, 0, KATANA_MAX_ENTITIES * sizeof(vec2f_t));
        if (!tracked_pos_found) {
                min_pos.x = 30.0f;
                min_pos.y = 30.0f;
        }

        vec2f_t camera_edge_buffer = {8.0f, 8.0f};
        min_pos = vec2f_sub(min_pos, camera_edge_buffer);
        max_pos = vec2f_add(max_pos, camera_edge_buffer);
        vec2f_t new_camera_pos = vec2f_div(vec2f_add(min_pos, max_pos), 2.0f);

        f32 cam_move_speed = 4.0f;
        game_state->world.camera.position =
            vec2f_add(vec2f_mul(game_state->world.camera.position, (1.0f - (input->delta_time * cam_move_speed))),
                      vec2f_mul(new_camera_pos, (input->delta_time * cam_move_speed)));

        vec2f_t screen_span = vec2f_sub(vec2f_add(max_pos, camera_edge_buffer), min_pos);
        f32 x_units_to_pixels = frame_buffer->width / screen_span.x;
        f32 y_units_to_pixels = frame_buffer->height / screen_span.y;

        f32 units_to_pixels = x_units_to_pixels < y_units_to_pixels ? x_units_to_pixels : y_units_to_pixels;
        f32 cam_zoom_speed = 2.0f;
        game_state->world.camera.units_to_pixels =
            (game_state->world.camera.units_to_pixels * (1.0f - (input->delta_time * cam_zoom_speed))) +
            (units_to_pixels * (input->delta_time * cam_zoom_speed));

#if 0
        // Clamp draw offset.
        if (game_state->world.camera.position.x < 0.0f) {
                game_state->world.camera.position.x = 0.0;
        } else if (game_state->world.camera.position.x + frame_buffer->width / units_to_pixels >=
                   32 * game_state->world.tilemap.tile_size.x) {
                game_state->world.camera.position.x =
                    (32 * game_state->world.tilemap.tile_size.x) - frame_buffer->width / units_to_pixels;
        }
        if (game_state->world.camera.position.y < 0.0f) {
                game_state->world.camera.position.y = 0.0;
        } else if (game_state->world.camera.position.y + frame_buffer->height / units_to_pixels >=
                   18 * game_state->world.tilemap.tile_size.y) {
                game_state->world.camera.position.y =
                    (18 * game_state->world.tilemap.tile_size.y) - frame_buffer->height / units_to_pixels;
        }
#endif

        vec2f_t background_pos = {64.0f, 36.0f};
        vec2f_t background_size = {128.0f, 72.0f};
        draw_image(background_pos, background_size, &game_state->world.camera, &game_state->background_image,
                   frame_buffer, 0);

        tilemap_t *tilemap = &game_state->world.tilemap;
        for (u32 i = 0; i < 18; ++i) {
                for (u32 j = 0; j < 32; ++j) {
                        if (tilemap->tiles[j + i * tilemap->tiles_wide]) {
                                vec2f_t tile_origin;
                                tile_origin.x = j * tilemap->tile_size.x;
                                tile_origin.y = i * tilemap->tile_size.y;
                                vec2f_t tile_half_size = vec2f_div(tilemap->tile_size, 2.0f);
                                tile_origin = vec2f_add(tile_origin, tile_half_size);
                                draw_image(tile_origin, tilemap->tile_size, &game_state->world.camera,
                                           &game_state->tile_image, frame_buffer, 1);
                        }
                }
        }

        for (u32 i = 0; i < KATANA_MAX_CONTROLLERS; ++i) {
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
                                        f32 anim_fps = katana_absf(input->controllers[i].left_stick_x) * anim->fps;
                                        if (anim->accumulator + input->delta_time >= (1.0f / anim_fps)) {
                                                anim->current_frame = ++anim->current_frame % anim->max_frames;
                                                anim->accumulator = 0;
                                        } else {
                                                anim->accumulator += input->delta_time;
                                        }
                                }
                        }
                        vec2f_t draw_offset = {-0.3f, -0.4f};
                        vec2f_t size = {8.0f, 5.0f};
                        vec2f_t draw_pos = vec2f_add(entity->position, draw_offset);
                        draw_image(draw_pos, size, &game_state->world.camera, &anim->frames[anim->current_frame],
                                   frame_buffer, entity->velocity.x > 0);

                        // Debug drawing for katana point.
                        if (entity->player.attacking) {
                                vec2f_t katana_pos;
                                if (entity->velocity.x > 0) {
                                        katana_pos = vec2f_add(entity->position, entity->player.katana_offset);
                                } else {
                                        katana_pos = vec2f_sub(entity->position, entity->player.katana_offset);
                                }
                                vec2f_t size = {1.0f, 1.0f};
                                draw_block(katana_pos, size, &game_state->world.camera, frame_buffer, 0xFFFFFFFF);
                        }
                }

                if (entity->type == entity_type_player && entity->player.teleporter_index) {
                        entity_t *teleporter = &game_state->world.entities[entity->player.teleporter_index];
                        assert(teleporter->type == entity_type_teleporter);
                        draw_image(teleporter->position, teleporter->size, &game_state->world.camera,
                                   teleporter->teleporter.image, frame_buffer, 0);
                }
        }

        output_sine_wave(game_state, audio);
}
