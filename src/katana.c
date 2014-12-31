#include "katana_types.h"

#include <math.h>

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

static void draw_block(i32 x, i32 y, u32 width, u32 height, game_frame_buffer_t *frame_buffer, u32 color)
{
        i32 right = x + width;
        i32 top = y + height;
        for (i32 i = x; i < right; ++i) {
                for (i32 j = y; j < top; ++j) {
                        frame_buffer->pixels[i + j * frame_buffer->width] = color;
                }
        }
}

unsigned char *get_tile(tilemap_t *tilemap, u32 x, u32 y) { return &tilemap->tiles[x + y * tilemap->tiles_wide]; }

b8 ray_cast_vertical(vec2_t origin, i32 end_y, tilemap_t *tilemap, i32 *intersect)
{
        assert(intersect);

        u32 tile_width = tilemap->tile_width;
        u32 tile_height = tilemap->tile_height;
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

b8 ray_cast_horizontal(vec2_t origin, i32 end_x, tilemap_t *tilemap, i32 *intersect)
{
        assert(intersect);

        u32 tile_width = tilemap->tile_width;
        u32 tile_height = tilemap->tile_height;
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

void update_player_position(game_state_t *game_state, game_input_t *input)
{
        u32 stick_value_count = input->controllers[0].stick_value_count;
        for (u32 i = 0; i < stick_value_count; ++i) {
                game_state->tone_hz += 10 * input->controllers[0].stick_y[i];

                f32 stick_x = input->controllers[0].stick_x[i];
                f32 stick_y = input->controllers[0].stick_y[i];

                u32 player_width = game_state->player_width;
                u32 player_height = game_state->player_height;
                u32 player_speed = game_state->player_speed;
                u8 tile_width = game_state->tilemap.tile_width;
                u8 tile_height = game_state->tilemap.tile_height;

                if (stick_x > 0.0f) {
                        i32 new_player_x = game_state->player_x + (player_speed * stick_x);
                        vec2_t origin = {};
                        origin.x = game_state->player_x + player_width;
                        origin.y = game_state->player_y;

                        i32 intersect_x1;
                        b8 x1_intersected = ray_cast_horizontal(origin, new_player_x + player_width,
                                                                &game_state->tilemap, &intersect_x1);
                        i32 intersect_x2;
                        origin.y = game_state->player_y + player_height - 1;
                        b8 x2_intersected = ray_cast_horizontal(origin, new_player_x + player_width,
                                                                &game_state->tilemap, &intersect_x2);

                        if (x1_intersected && x2_intersected) {
                                if (x1_intersected <= x2_intersected) {
                                        game_state->player_x = intersect_x1 - player_width;
                                } else {
                                        game_state->player_x = intersect_x2 - player_width;
                                }
                        } else if (x1_intersected) {
                                game_state->player_x = intersect_x1 - player_width;
                        } else if (x2_intersected) {
                                game_state->player_x = intersect_x2 - player_width;
                        } else {
                                game_state->player_x = new_player_x;
                        }
                }
                if (stick_x < 0.0f) {
                        i32 new_player_x = game_state->player_x + (player_speed * stick_x);
                        vec2_t origin = {};
                        origin.x = game_state->player_x;
                        origin.y = game_state->player_y;

                        i32 intersect_x1;
                        b8 x1_intersected =
                            ray_cast_horizontal(origin, new_player_x, &game_state->tilemap, &intersect_x1);
                        i32 intersect_x2;
                        origin.y = game_state->player_y + player_height - 1;
                        b8 x2_intersected =
                            ray_cast_horizontal(origin, new_player_x, &game_state->tilemap, &intersect_x2);

                        if (x1_intersected && x2_intersected) {
                                if (x1_intersected <= x2_intersected) {
                                        game_state->player_x = intersect_x1 + tile_width;
                                } else {
                                        game_state->player_x = intersect_x2 + tile_width;
                                }
                        } else if (x1_intersected) {
                                game_state->player_x = intersect_x1 + tile_width;
                        } else if (x2_intersected) {
                                game_state->player_x = intersect_x2 + tile_width;
                        } else {
                                game_state->player_x = new_player_x;
                        }
                }
                if (stick_y > 0.0f) {
                        i32 new_player_y = game_state->player_y + (player_speed * stick_y);
                        vec2_t origin = {};
                        origin.x = game_state->player_x;
                        origin.y = game_state->player_y + player_height;

                        i32 intersect_y1;
                        b8 y1_intersected = ray_cast_vertical(origin, new_player_y + player_height,
                                                              &game_state->tilemap, &intersect_y1);
                        i32 intersect_y2;
                        origin.x = game_state->player_x + player_width - 1;
                        b8 y2_intersected = ray_cast_vertical(origin, new_player_y + player_height,
                                                              &game_state->tilemap, &intersect_y2);

                        if (y1_intersected && y2_intersected) {
                                if (y1_intersected <= y2_intersected) {
                                        game_state->player_y = intersect_y1 - player_height;
                                } else {
                                        game_state->player_y = intersect_y2 - player_height;
                                }
                        } else if (y1_intersected) {
                                game_state->player_y = intersect_y1 - player_height;
                        } else if (y2_intersected) {
                                game_state->player_y = intersect_y2 - player_height;
                        } else {
                                game_state->player_y = new_player_y;
                        }
                }
                if (stick_y < 0.0f) {
                        i32 new_player_y = game_state->player_y + (player_speed * stick_y);
                        vec2_t origin = {};
                        origin.x = game_state->player_x;
                        origin.y = game_state->player_y;

                        i32 intersect_y1;
                        b8 y1_intersected =
                            ray_cast_vertical(origin, new_player_y, &game_state->tilemap, &intersect_y1);
                        i32 intersect_y2;
                        origin.x = game_state->player_x + player_width - 1;
                        b8 y2_intersected =
                            ray_cast_vertical(origin, new_player_y, &game_state->tilemap, &intersect_y2);

                        if (y1_intersected && y2_intersected) {
                                if (y1_intersected <= y2_intersected) {
                                        game_state->player_y = intersect_y1 + tile_height;
                                } else {
                                        game_state->player_y = intersect_y2 + tile_height;
                                }
                        } else if (y1_intersected) {
                                game_state->player_y = intersect_y1 + tile_height;
                        } else if (y2_intersected) {
                                game_state->player_y = intersect_y2 + tile_height;
                        } else {
                                game_state->player_y = new_player_y;
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
                game_state->player_x = 100;
                game_state->player_y = 50;
                game_state->player_width = 20;
                game_state->player_height = 20;
                game_state->player_speed = 10;
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
                game_state->tilemap.tiles = (unsigned char *)tilemap;
                game_state->tilemap.tile_width = 40;
                game_state->tilemap.tile_height = 40;
                game_state->tilemap.tiles_wide = 32;
                game_state->tilemap.tiles_high = 18;
                memory->is_initialized = 1;
        }

        update_player_position(game_state, input);

        for (u32 i = 0; i < 18; ++i) {
                for (u32 j = 0; j < 32; ++j) {
                        if (game_state->tilemap.tiles[j + i * game_state->tilemap.tiles_wide]) {
                                draw_block(j * game_state->tilemap.tile_width, i * game_state->tilemap.tile_height,
                                           game_state->tilemap.tile_width, game_state->tilemap.tile_height,
                                           frame_buffer, 0xFFFFFF00);
                        }
                }
        }

        draw_block(game_state->player_x, game_state->player_y, game_state->player_width, game_state->player_height,
                   frame_buffer, 0xFFFFFFFF);

        output_sine_wave(game_state, audio);
}
