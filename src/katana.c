#include "katana.h"

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

static void render_weird_gradient(game_frame_buffer_t *frame_buffer, i32 blue_offset, i32 green_offset)
{
        // TODO(casey): Let's see what the optimizer does

        u8 *row = (u8 *)frame_buffer->pixels;
        for (u32 y = 0; y < frame_buffer->height; ++y) {
                u32 *pixel = (u32 *)row;
                for (u32 x = 0; x < frame_buffer->width; ++x) {
                        u8 blue = (u8)(x + blue_offset);
                        u8 green = (u8)(y + green_offset);

                        *pixel++ = ((green << 16) | blue);
                }

                row += (frame_buffer->width * 4);
        }
}

static void draw_block(i32 x, i32 y, game_frame_buffer_t *frame_buffer, u32 color)
{
        i32 right = x + 40;
        i32 top = y + 40;
        for (i32 i = x; i < right; ++i) {
                for (i32 j = y; j < top; ++j) {
                        frame_buffer->pixels[i + j * frame_buffer->width] = color;
                }
        }
}

void game_update_and_render(game_memory_t *memory, game_frame_buffer_t *frame_buffer, game_audio_t *audio,
                            game_input_t *input, game_output_t *output)
{
        if (!memory) {
                return;
        }

        render_weird_gradient(frame_buffer, 200, 10);

        game_state_t *game_state = (game_state_t *)memory->transient_store;
        if (!memory->is_initialized) {
                game_state->block_x = 100;
                game_state->block_y = 50;
                game_state->t_sine = 0.0f;
                game_state->tone_hz = 512;
                memory->is_initialized = 1;
        }
        u32 stick_value_count = input->controllers[0].stick_value_count;
        for (u32 i = 0; i < stick_value_count; ++i) {
                game_state->tone_hz += 10 * input->controllers[0].stick_y[i];
                i32 new_x_pos = game_state->block_x + (10 * input->controllers[0].stick_x[i]);
                i32 new_y_pos = game_state->block_y + (10 * input->controllers[0].stick_y[i]);
                if (new_x_pos <= 0 || (new_x_pos + 50) >= (i32)frame_buffer->width || new_y_pos <= 0 ||
                    (new_y_pos + 50) >= (i32)frame_buffer->height) {
                        output->controllers[0].rumble = 0;
                        output->controllers[0].intensity = 0.5f;
                } else {
                        game_state->block_x = new_x_pos;
                        game_state->block_y = new_y_pos;
                }
        }

        unsigned char tile_map[18][32] = {
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

        draw_block(game_state->block_x, game_state->block_y, frame_buffer, 0xFFFFFFFF);

        for (u32 i = 0; i < 18; ++i) {
                for (u32 j = 0; j < 32; ++j) {
                        if (tile_map[i][j]) {
                                draw_block(j * 40, i * 40, frame_buffer, 0xFFFFFF00);
                        }
                }
        }

        output_sine_wave(game_state, audio);
}
