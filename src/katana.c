#include "katana.h"

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

static void draw_block(i32 x, i32 y, game_frame_buffer_t *frame_buffer)
{
        i32 right = x + 30;
        i32 top = y + 30;
        u32 color = 0xFFFFFFFF;
        for (i32 i = x; i < right; ++i) {
                for (i32 j = y; j < top; ++j) {
                        frame_buffer->pixels[i + j * frame_buffer->width] = color;
                }
        }
}

void game_update_and_render(game_memory_t *memory, game_frame_buffer_t *frame_buffer, game_input_t *input,
                            game_output_t *output)
{
        if (!memory) {
                return;
        }

        render_weird_gradient(frame_buffer, 200, 10);

        game_state_t *game_state = (game_state_t *)memory->transient_store;
        static i32 first_time = 1;
        if (first_time) {
                game_state->block_x = 100;
                game_state->block_y = 50;
                first_time = 0;
        }
        u32 stick_value_count = input->controllers[0].stick_value_count;
        for (u32 i = 0; i < stick_value_count; ++i) {
                i32 new_x_pos = game_state->block_x + (10 * input->controllers[0].stick_x[i]);
                i32 new_y_pos = game_state->block_y + (10 * input->controllers[0].stick_y[i]);
                if (new_x_pos <= 0 || (new_x_pos + 50) >= (i32)frame_buffer->width || new_y_pos <= 0 ||
                    (new_y_pos + 50) >= (i32)frame_buffer->height) {
                        output->controllers[0].rumble = 1;
                        output->controllers[0].intensity = 0.5f;
                } else {
                        game_state->block_x = new_x_pos;
                        game_state->block_y = new_y_pos;
                }
        }

        draw_block(game_state->block_x, game_state->block_y, frame_buffer);
}
