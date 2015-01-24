#include "katana_types.h"
#include "katana_intrinsics.h"
#include "katana_math.h"
#include "katana_vec.c"

#define STB_ASSERT(x) assert(x);
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static void output_sine_wave(game_state_t *game_state, game_audio_t *audio)
{
        i16 tone_volume = 3000;
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
        f32 dest_b = (f32)((*dest >> 16) & 0xFF);
        f32 dest_g = (f32)((*dest >> 8) & 0xFF);
        f32 dest_r = (f32)((*dest >> 0) & 0xFF);

        f32 src_a = (f32)((src >> 24) & 0xFF) / 255.0f;
        f32 src_b = (f32)((src >> 16) & 0xFF);
        f32 src_g = (f32)((src >> 8) & 0xFF);
        f32 src_r = (f32)((src >> 0) & 0xFF);

        f32 b = (1.0f - src_a) * dest_b + (src_a * src_b);
        f32 g = (1.0f - src_a) * dest_g + (src_a * src_g);
        f32 r = (1.0f - src_a) * dest_r + (src_a * src_r);

        *dest = (u32)(b + 0.5f) << 16 | (u32)(g + 0.5f) << 8 | (u32)(r + 0.5f) << 0;
}

static void draw_image(vec2f_t pos, vec2f_t size, vec2f_t draw_offset, image_t *image,
                       game_frame_buffer_t *frame_buffer, f32 units_to_pixels, b8 flip_x)
{
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
        top_left_corner = vec2f_sub(top_left_corner, draw_offset);
        bot_right_corner = vec2f_sub(bot_right_corner, draw_offset);

        // Convert to pixel values and round to nearest integer.
        top_left_corner = vec2f_mul(top_left_corner, units_to_pixels);
        bot_right_corner = vec2f_mul(bot_right_corner, units_to_pixels);
        vec2i_t top_left_pixel;
        vec2i_t bot_right_pixel;
        vec2f_floor2(top_left_corner, bot_right_corner, &top_left_pixel, &bot_right_pixel);

        // Bounds checking
        i32 image_data_offset_x = 0;
        i32 image_data_offset_y = 0;
        if (top_left_pixel.x < 0) {
                image_data_offset_x = -top_left_pixel.x;
                top_left_pixel.x = 0;
        } else if (top_left_pixel.x > (i32)frame_buffer->width) {
                top_left_pixel.x = frame_buffer->width;
        }
        if (top_left_pixel.y < 0) {
                image_data_offset_y = -top_left_pixel.y;
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
                        u32 sample_y = (i - top_left_pixel.y) * y_ratio;
                        for (i32 j = top_left_pixel.x; j < bot_right_pixel.x; ++j) {
                                u32 *dest = &frame_buffer->pixels[j + i * frame_buffer->width];
                                u32 sample_x = (bot_right_pixel.x - j) * x_ratio;
                                linear_blend(image_data[sample_x + sample_y * image->width], dest);
                        }
                }
        } else {
                for (i32 i = top_left_pixel.y; i < bot_right_pixel.y; ++i) {
                        u32 sample_y = (i - top_left_pixel.y) * y_ratio;
                        for (i32 j = top_left_pixel.x; j < bot_right_pixel.x; ++j) {
                                u32 *dest = &frame_buffer->pixels[j + i * frame_buffer->width];
                                u32 sample_x = (j - top_left_pixel.x) * x_ratio;
                                linear_blend(image_data[sample_x + sample_y * image->width], dest);
                        }
                }
        }
}

static void draw_block(vec2f_t pos, vec2f_t size, vec2f_t draw_offset, game_frame_buffer_t *frame_buffer, u32 color,
                       f32 units_to_pixels)
{
        // Calculate top left and bottom right of block.
        vec2f_t half_size = vec2f_div(size, 2.0f);
        vec2f_t top_left_corner = vec2f_sub(pos, half_size);
        vec2f_t bot_right_corner = vec2f_add(pos, half_size);

        // Add draw offsets.
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
                        frame_buffer->pixels[j + i * frame_buffer->width] = color;
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
        // No intersect found if we get here so just set it to as far as requested.
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
        // No intersect found if we get here so just set it to as far as requested.
        result.intersect = end_x;
        result.did_intersect = 0;
        return result;
}

static vec2i_t update_player_position(world_t *world, game_input_t *input)
{
        player_t *player = &world->player;

        // Ensures we never move exactly onto the tile we are colliding with.
        f32 collision_buffer = 0.001f;
        vec2f_t new_accel = {};

        vec2f_t half_player_size = vec2f_div(player->size, 2.0f);

        // Gravity
        new_accel = vec2f_add(new_accel, world->gravity);

        // Jump
        if (input->controllers[0].action_down.ended_down && player->on_ground) {
                player->velocity.y = -40.0f;
        }

        // NOTE(Wes): We always ray cast 4 times for movement:
        // If we are moving left we cast left from the left side at the top and bottom of the player, vice versa
        // for
        // right.
        // If we are moving down we cast down from the bottom side at the left and right of the player, vice
        // versa for
        // up.
        vec2f_t left_stick;
        vec2f_t right_stick;
        left_stick.x = input->controllers[0].left_stick_x;
        left_stick.y = 0.0f;
        right_stick.x = input->controllers[0].right_stick_x;
        right_stick.y = input->controllers[0].right_stick_y;

        // TODO(Wes): Create an offset speed instead of using player speed.
        right_stick = vec2f_mul(right_stick, 0.5f);
        world->draw_offset = vec2f_add(right_stick, world->draw_offset);

        // Friction force. USE ODE here!
        vec2f_t friction = {-7.0f * player->velocity.x, 0.0f};
        new_accel = vec2f_add(new_accel, friction);

        // Verlet integration.
        f32 acceleration_factor = 150.0f;
        new_accel = vec2f_add(vec2f_mul(left_stick, acceleration_factor), new_accel);
        vec2f_t average_accel = vec2f_div(vec2f_add(player->acceleration, new_accel), 2);
        vec2f_t new_player_pos =
            vec2f_mul(vec2f_mul(player->acceleration, 0.5f), input->delta_time * input->delta_time);
        new_player_pos = vec2f_add(vec2f_mul(player->velocity, input->delta_time), new_player_pos);
        new_player_pos = vec2f_add(player->position, new_player_pos);
        player->velocity = vec2f_add(vec2f_mul(average_accel, input->delta_time), player->velocity);

        // Get the movement direction as -1 = left/up, 0 = unused, 1 = right/down
        vec2i_t move_dir = vec2f_sign(player->velocity);

        // Horizontal collision check.
        f32 cast_x = half_player_size.x * move_dir.x;
        vec2f_t top_origin;
        top_origin.x = player->position.x + cast_x;
        top_origin.y = player->position.y - half_player_size.y + collision_buffer;
        vec2f_t bot_origin;
        bot_origin.x = top_origin.x;
        bot_origin.y = player->position.y + half_player_size.y - collision_buffer;

        f32 end_x = new_player_pos.x + cast_x;
        ray_cast_result intersect_x1 = ray_cast_horizontal(top_origin, end_x, &world->tilemap);
        ray_cast_result intersect_x2 = ray_cast_horizontal(bot_origin, end_x, &world->tilemap);

        if ((move_dir.x * intersect_x1.intersect) <= (move_dir.x * intersect_x2.intersect)) {
                player->position.x = intersect_x1.intersect - cast_x;
        } else {
                player->position.x = intersect_x2.intersect - cast_x;
        }

        if (intersect_x1.did_intersect || intersect_x2.did_intersect) {
                player->velocity.x = 0.0f;
        }

        // Vertical collision check.
        f32 cast_y = half_player_size.y * move_dir.y;
        vec2f_t left_origin;
        left_origin.x = player->position.x - half_player_size.x + collision_buffer;
        left_origin.y = player->position.y + cast_y;
        vec2f_t right_origin;
        right_origin.x = player->position.x + half_player_size.x - collision_buffer;
        right_origin.y = left_origin.y;

        f32 end_y = new_player_pos.y + cast_y;
        ray_cast_result intersect_y1 = ray_cast_vertical(left_origin, end_y, &world->tilemap);
        ray_cast_result intersect_y2 = ray_cast_vertical(right_origin, end_y, &world->tilemap);

        if ((move_dir.y * intersect_y1.intersect) <= (move_dir.y * intersect_y2.intersect)) {
                player->position.y = intersect_y1.intersect - cast_y;
        } else {
                player->position.y = intersect_y2.intersect - cast_y;
        }

        if (intersect_y1.did_intersect || intersect_y2.did_intersect) {
                player->velocity.y = 0.0f;
                if (move_dir.y == 1) {
                        player->on_ground = 1;
                } else {
                        player->on_ground = 0;
                }
        } else {
                player->on_ground = 0;
        }

        return move_dir;
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

        game_state_t *game_state = (game_state_t *)memory->transient_store;
        if (!memory->is_initialized) {
                game_state->world.units_to_pixels = 20.0f / 1.0f;
                game_state->world.draw_offset.x = 0.0f;
                game_state->world.draw_offset.y = 0.0f;
                game_state->world.gravity.x = 0.0f;
                game_state->world.gravity.y = 9.8f * 20;
                game_state->t_sine = 0.0f;
                game_state->tone_hz = 512;
                game_state->world.player.position.x = 20.0f;
                game_state->world.player.position.y = 20.0f;
                game_state->world.player.size.x = 2.0f;
                game_state->world.player.size.y = 4.0f;
                game_state->world.player.on_ground = 0;
                game_state->world.player.anim_fps = 24.0f;
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

                memory->is_initialized = 1;
        }

        player_t *player = &game_state->world.player;

        if (input->controllers[0].left_shoulder.ended_down) {
                game_state->world.units_to_pixels -= 0.1f;
        } else if (input->controllers[0].right_shoulder.ended_down) {
                game_state->world.units_to_pixels += 0.1f;
        }

        f32 units_to_pixels = game_state->world.units_to_pixels;
        game_state->world.draw_offset = vec2f_copy(player->position);
        game_state->world.draw_offset.x -= frame_buffer->width / units_to_pixels / 2;
        game_state->world.draw_offset.y -= frame_buffer->height / units_to_pixels / 2;

        vec2f_t background_pos = {64.0f, 36.0f};
        vec2f_t background_size = {128.0f, 72.0f};
        draw_image(background_pos, background_size, game_state->world.draw_offset, &game_state->background_image,
                   frame_buffer, units_to_pixels, 0);

        vec2i_t move_dir = update_player_position(&game_state->world, input);

        tilemap_t *tilemap = &game_state->world.tilemap;
        for (u32 i = 0; i < 18; ++i) {
                for (u32 j = 0; j < 32; ++j) {
                        if (tilemap->tiles[j + i * tilemap->tiles_wide]) {
                                vec2f_t tile_origin;
                                tile_origin.x = j * tilemap->tile_size.x;
                                tile_origin.y = i * tilemap->tile_size.y;
                                vec2f_t tile_half_size = vec2f_div(tilemap->tile_size, 2.0f);
                                tile_origin = vec2f_add(tile_origin, tile_half_size);
                                draw_image(tile_origin, tilemap->tile_size, game_state->world.draw_offset,
                                           &game_state->tile_image, frame_buffer, units_to_pixels, 0);
                        }
                }
        }

        // draw_block(player->position, player->size, game_state->world.draw_offset, frame_buffer, 0xFFFFFFFF,
        //          units_to_pixels);

        vec2f_t player_draw_offset = {-0.3f, -0.4f};
        vec2f_t player_draw_pos = vec2f_add(player->position, player_draw_offset);
        vec2f_t player_size = {8.0f, 5.0f};
        image_t *player_image = 0;
        if (input->controllers[0].left_stick_x == 0.0f) {
                player_image = &game_state->player_images[player->anim_frame % 6];
        } else {
                f32 anim_fps = katana_absf(input->controllers[0].left_stick_x) * player->anim_fps;
                if (player->anim_accumulator + input->delta_time >= (1.0f / anim_fps)) {
                        player->anim_frame++;
                        player->anim_accumulator = 0;
                } else {
                        player->anim_accumulator += input->delta_time;
                }
                player_image = &game_state->player_images[player->anim_frame % 6];
        }

        draw_image(player_draw_pos, player_size, game_state->world.draw_offset, player_image, frame_buffer,
                   units_to_pixels, move_dir.x > 0);

        output_sine_wave(game_state, audio);
}
