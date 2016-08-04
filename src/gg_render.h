#pragma once

#include "gg_vec.h"

// Converts a normalized color into a texture specific pixel value.
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

// Renders the specified image.
void render_push_image(render_queue_t *queue,
                       basis_t *basis,
                       v4 tint,
                       image_t *image,
                       image_t *normals,
                       light_t *lights,
                       int num_lights);

// Renders the a rect at the start (top, left) of the specified size and color.
void render_push_rect(render_queue_t *queue, basis_t *basis, v4 color);

// Renders the a rect at the start (top, left) of the specified size and color.
// It will be hollow and have a border of the specified size.
void render_push_hollow_rect(render_queue_t *queue, basis_t *basis, v4 color, f32 border_size);

// Allocates a render queue.
render_queue_t *render_alloc_queue(memory_arena_t *arena, u32 max_render_queue_size, camera_t *cam);

// Performs drawing on all render commands in the queue.
void render_draw_queue(render_queue_t *queue, game_frame_buffer_t *frame_buffer, game_work_queues_t *work_queues);
