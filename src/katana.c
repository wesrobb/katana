#include "katana.h"

static void draw_block(i32 x, i32 y, game_frame_buffer_t *frame_buffer)
{
	i32 right = x + 50;
	i32 top = y + 50;
	u32 color = 0xFFFFFFFF;
	for (i32 i = x; i < right; ++i) {
		for (i32 j = y; j < top; ++j) {
			frame_buffer->pixels[j + i * frame_buffer->width] =
			    color;
		}
	}
}

void game_update_and_render(game_memory_t *memory,
			    game_frame_buffer_t *frame_buffer)
{
	if (!memory) {
		return;
	}
	draw_block(50, 50, frame_buffer);
}
