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

                        *pixel++ = ((green << 8) | blue);
                }

                row += (frame_buffer->width * 4);
        }
}

static void draw_block(i32 x, i32 y, game_frame_buffer_t *frame_buffer)
{
        i32 right = x + 50;
        i32 top = y + 50;
        u32 color = 0xFFFFFFFF;
        for (i32 i = x; i < right; ++i) {
                for (i32 j = y; j < top; ++j) {
                        frame_buffer->pixels[j + i * frame_buffer->width] = color;
                }
        }
}

void game_update_and_render(game_memory_t *memory, game_frame_buffer_t *frame_buffer)
{
        if (!memory) {
                return;
        }
        render_weird_gradient(frame_buffer, 200, 10);
        draw_block(100, 50, frame_buffer);
}
