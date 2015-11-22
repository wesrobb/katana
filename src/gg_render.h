#pragma once

#include "gg_vec.h"

// Converts a normalized color into a texture specific pixel value.
// Note that this should not be used for writing values into the
// framebuffer as the order of the bytes differs.
u32 color_image_32(v4 color);

// Reads normalized color values out of a pixel from a texture.
v4 read_image_color(u32 buffer);

// Adjusts the specified color for gamma correction.
// As performing blending in srgb space generally produces
// results that a slightly inaccurate.
v4 srgb_to_linear(v4 color);
v4 linear_to_srgb(v4 color);

// Clears the screen to the specified color.
void render_push_clear(render_queue_t *queue, v4 color);

void render_push_block(render_queue_t *queue, v2 pos, v2 size, v4 tint);

// Renders the specified image.
void render_push_rotated_block(render_queue_t *queue,
                               basis_t *basis,
                               v4 tint,
                               image_t *image,
                               image_t *normals,
                               light_t *lights,
                               int num_lights);

void render_push_line(render_queue_t *queue, basis_t *basis, v2 start, v2 end, f32 width, v4 color);

// Allocates a render queue.
render_queue_t *render_alloc_queue(memory_arena_t *arena, u32 max_render_queue_size, camera_t *cam);

// Performs drawing on all render commands in the queue.
void render_draw_queue(render_queue_t *queue, game_frame_buffer_t *frame_buffer);
