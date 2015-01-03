#include "katana_types.h"
#include "katana_intrinsics.h"

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

static void render_background(game_frame_buffer_t *frame_buffer, i32 blue_offset, i32 green_offset)
{
        u8 *row = (u8 *)frame_buffer->pixels;
        for (u32 y = 0; y < frame_buffer->height; ++y) {
                u32 *pixel = (u32 *)row;
                for (u32 x = 0; x < frame_buffer->width; ++x) {
                        u8 blue = (u8)(x + blue_offset);
                        u8 green = (u8)(y + green_offset);

                        *pixel++ = ((green << 8) | blue);
                }

                row += (frame_buffer->width * 4);
        }
}

static void draw_block(f32 x, f32 y, f32 width, f32 height, game_frame_buffer_t *frame_buffer, u32 color,
                       f32 units_to_pixels)
{
        f32 round_input[4];
        f32 round_output[4];
        round_input[0] = x * units_to_pixels;
        round_input[1] = y * units_to_pixels;
        round_input[2] = (x + width) * units_to_pixels;
        round_input[3] = (y + height) * units_to_pixels;
        k_roundf4(round_input, round_output);
        i32 x_start = (i32)round_output[0];
        i32 y_start = (i32)round_output[1];
        i32 right = (i32)round_output[2];
        i32 top = (i32)round_output[3];
        for (i32 i = x_start; i < right; ++i) {
                for (i32 j = y_start; j < top; ++j) {
                        frame_buffer->pixels[i + j * frame_buffer->width] = color;
                }
        }
}

unsigned char *get_tile(tilemap_t *tilemap, u32 x, u32 y)
{
        return &tilemap->tiles[x + y * tilemap->tiles_wide];
}

b8 ray_cast_vertical(vec2f_t origin, f32 end_y, tilemap_t *tilemap, f32 *intersect)
{
        assert(intersect);

        f32 tile_width = tilemap->tile_width;
        f32 tile_height = tilemap->tile_height;
        i32 tilemap_start_y = origin.y / tile_height;
        i32 tilemap_x = origin.x / tile_width;
        i32 tilemap_end_y = end_y / tile_height;

        if (origin.y < end_y) {
                for (i32 i = tilemap_start_y; i <= tilemap_end_y; ++i) {
                        if (*get_tile(tilemap, tilemap_x, i)) {
                                *intersect = i * tile_height;
                                return 1;
                        }
                }
        } else {
                for (i32 i = tilemap_start_y; i >= tilemap_end_y; --i) {
                        if (*get_tile(tilemap, tilemap_x, i)) {
                                *intersect = i * tile_height;
                                return 1;
                        }
                }
        }
        return 0;
}

b8 ray_cast_horizontal(vec2f_t origin, f32 end_x, tilemap_t *tilemap, f32 *intersect)
{
        assert(intersect);

        f32 tile_width = tilemap->tile_width;
        f32 tile_height = tilemap->tile_height;
        i32 tilemap_start_x = origin.x / tile_width;
        i32 tilemap_y = origin.y / tile_height;
        i32 tilemap_end_x = end_x / tile_width;

        if (origin.x < end_x) {
                for (i32 i = tilemap_start_x; i <= tilemap_end_x; ++i) {
                        if (*get_tile(tilemap, i, tilemap_y)) {
                                *intersect = (i * tile_width);
                                return 1;
                        }
                }
        } else {
                for (i32 i = tilemap_start_x; i >= tilemap_end_x; --i) {
                        if (*get_tile(tilemap, i, tilemap_y)) {
                                *intersect = i * tile_width;
                                return 1;
                        }
                }
        }
        return 0;
}

void update_player_position(world_t *world, game_input_t *input)
{
        u32 player_width = world->player_size.x;
        u32 player_height = world->player_size.y;
        u32 player_speed = world->player_speed;
        u8 tile_width = world->tilemap.tile_width;
        u8 tile_height = world->tilemap.tile_height;

        // Gravity
        f32 new_player_y = world->player_pos.y + (world->gravity * input->delta_time);
        vec2f_t origin = {};
        origin.x = world->player_pos.x;
        origin.y = world->player_pos.y + player_height;

        f32 intersect_y1;
        b8 y1_intersected = ray_cast_vertical(origin, new_player_y + player_height, &world->tilemap, &intersect_y1);
        f32 intersect_y2;
        origin.x = world->player_pos.x + player_width - 0.1f;
        b8 y2_intersected = ray_cast_vertical(origin, new_player_y + player_height, &world->tilemap, &intersect_y2);

        if (y1_intersected && y2_intersected) {
                if (y1_intersected <= y2_intersected) {
                        world->player_pos.y = intersect_y1 - player_height;
                } else {
                        world->player_pos.y = intersect_y2 - player_height;
                }
        } else if (y1_intersected) {
                world->player_pos.y = intersect_y1 - player_height;
        } else if (y2_intersected) {
                world->player_pos.y = intersect_y2 - player_height;
        } else {
                world->player_pos.y = new_player_y;
        }

        u32 stick_value_count = input->controllers[0].stick_value_count;
        for (u32 i = 0; i < stick_value_count; ++i) {

                f32 stick_x = input->controllers[0].stick_x[i];
                f32 stick_y = input->controllers[0].stick_y[i];

                if (stick_x > 0.0f) {
                        f32 new_player_x = world->player_pos.x + (player_speed * stick_x);
                        vec2f_t origin = {};
                        origin.x = world->player_pos.x + player_width;
                        origin.y = world->player_pos.y;

                        f32 intersect_x1;
                        b8 x1_intersected =
                            ray_cast_horizontal(origin, new_player_x + player_width, &world->tilemap, &intersect_x1);
                        f32 intersect_x2;
                        origin.y = world->player_pos.y + player_height - 0.1f;
                        b8 x2_intersected =
                            ray_cast_horizontal(origin, new_player_x + player_width, &world->tilemap, &intersect_x2);

                        if (x1_intersected && x2_intersected) {
                                if (x1_intersected <= x2_intersected) {
                                        world->player_pos.x = intersect_x1 - player_width;
                                } else {
                                        world->player_pos.x = intersect_x2 - player_width;
                                }
                        } else if (x1_intersected) {
                                world->player_pos.x = intersect_x1 - player_width;
                        } else if (x2_intersected) {
                                world->player_pos.x = intersect_x2 - player_width;
                        } else {
                                world->player_pos.x = new_player_x;
                        }
                }
                if (stick_x < 0.0f) {
                        f32 new_player_x = world->player_pos.x + (player_speed * stick_x);
                        vec2f_t origin = {};
                        origin.x = world->player_pos.x;
                        origin.y = world->player_pos.y;

                        f32 intersect_x1;
                        b8 x1_intersected = ray_cast_horizontal(origin, new_player_x, &world->tilemap, &intersect_x1);
                        f32 intersect_x2;
                        origin.y = world->player_pos.y + player_height - 0.1f;
                        b8 x2_intersected = ray_cast_horizontal(origin, new_player_x, &world->tilemap, &intersect_x2);

                        if (x1_intersected && x2_intersected) {
                                if (x1_intersected <= x2_intersected) {
                                        world->player_pos.x = intersect_x1 + tile_width;
                                } else {
                                        world->player_pos.x = intersect_x2 + tile_width;
                                }
                        } else if (x1_intersected) {
                                world->player_pos.x = intersect_x1 + tile_width;
                        } else if (x2_intersected) {
                                world->player_pos.x = intersect_x2 + tile_width;
                        } else {
                                world->player_pos.x = new_player_x;
                        }
                }
                if (stick_y > 0.0f) {
                        f32 new_player_y = world->player_pos.y + (player_speed * stick_y);
                        vec2f_t origin = {};
                        origin.x = world->player_pos.x;
                        origin.y = world->player_pos.y + player_height;

                        f32 intersect_y1;
                        b8 y1_intersected =
                            ray_cast_vertical(origin, new_player_y + player_height, &world->tilemap, &intersect_y1);
                        f32 intersect_y2;
                        origin.x = world->player_pos.x + player_width - 0.1f;
                        b8 y2_intersected =
                            ray_cast_vertical(origin, new_player_y + player_height, &world->tilemap, &intersect_y2);

                        if (y1_intersected && y2_intersected) {
                                if (y1_intersected <= y2_intersected) {
                                        world->player_pos.y = intersect_y1 - player_height;
                                } else {
                                        world->player_pos.y = intersect_y2 - player_height;
                                }
                        } else if (y1_intersected) {
                                world->player_pos.y = intersect_y1 - player_height;
                        } else if (y2_intersected) {
                                world->player_pos.y = intersect_y2 - player_height;
                        } else {
                                world->player_pos.y = new_player_y;
                        }
                }
                if (stick_y < 0.0f) {
                        f32 new_player_y = world->player_pos.y + (player_speed * stick_y);
                        vec2f_t origin = {};
                        origin.x = world->player_pos.x;
                        origin.y = world->player_pos.y;

                        f32 intersect_y1;
                        b8 y1_intersected = ray_cast_vertical(origin, new_player_y, &world->tilemap, &intersect_y1);
                        f32 intersect_y2;
                        origin.x = world->player_pos.x + player_width - 0.1f;
                        b8 y2_intersected = ray_cast_vertical(origin, new_player_y, &world->tilemap, &intersect_y2);

                        if (y1_intersected && y2_intersected) {
                                if (y1_intersected <= y2_intersected) {
                                        world->player_pos.y = intersect_y1 + tile_height;
                                } else {
                                        world->player_pos.y = intersect_y2 + tile_height;
                                }
                        } else if (y1_intersected) {
                                world->player_pos.y = intersect_y1 + tile_height;
                        } else if (y2_intersected) {
                                world->player_pos.y = intersect_y2 + tile_height;
                        } else {
                                world->player_pos.y = new_player_y;
                        }
                }
        }
}

void game_update_and_render(game_memory_t *memory, game_frame_buffer_t *frame_buffer, game_audio_t *audio,
                            game_input_t *input, game_output_t *output)
{
        if (!memory) {
                return;
        }

        render_background(frame_buffer, 200, 10);

        game_state_t *game_state = (game_state_t *)memory->transient_store;
        if (!memory->is_initialized) {
                game_state->world.units_to_pixels = 10.0f / 1.0f;
                game_state->world.player_pos.x = 10.0f;
                game_state->world.player_pos.y = 10.0f;
                game_state->world.player_size.x = 2.0f;
                game_state->world.player_size.y = 2.0f;
                game_state->world.player_speed = 1.0f;
                game_state->world.gravity = 9.8f;
                game_state->t_sine = 0.0f;
                game_state->tone_hz = 512;
                static unsigned char tilemap[18][32] = {
                    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                    {1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
                    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
                    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                };
                game_state->world.tilemap.tiles = (unsigned char *)tilemap;
                game_state->world.tilemap.tile_width = 4.0f;
                game_state->world.tilemap.tile_height = 4.0f;
                game_state->world.tilemap.tiles_wide = 32;
                game_state->world.tilemap.tiles_high = 18;
                memory->is_initialized = 1;
        }

        if (input->controllers[0].left_shoulder.ended_down) {
                game_state->world.units_to_pixels -= 0.1f;
        } else if (input->controllers[0].right_shoulder.ended_down) {
                game_state->world.units_to_pixels += 0.1f;
        }

        update_player_position(&game_state->world, input);

        tilemap_t *tilemap = &game_state->world.tilemap;
        f32 units_to_pixels = game_state->world.units_to_pixels;
        for (u32 i = 0; i < 18; ++i) {
                for (u32 j = 0; j < 32; ++j) {
                        if (tilemap->tiles[j + i * tilemap->tiles_wide]) {
                                draw_block(j * tilemap->tile_width, i * tilemap->tile_height, tilemap->tile_width,
                                           tilemap->tile_height, frame_buffer, 0xFFFFFF00, units_to_pixels);
                        }
                }
        }

        draw_block(game_state->world.player_pos.x, game_state->world.player_pos.y, game_state->world.player_size.x,
                   game_state->world.player_size.y, frame_buffer, 0xFFFFFFFF, units_to_pixels);

        output_sine_wave(game_state, audio);
}
