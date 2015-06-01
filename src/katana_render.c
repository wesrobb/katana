#include "katana_types.h"

typedef enum { render_type_clear; render_type_block; render_type_image; }
render_type_t;

typedef struct {
        render_type_t type;
} render_cmd_header_t;

typedef struct {
        v4 color;
} render_cmd_clear_t;

typedef struct {
        v2 pos;
        v2 size;
        v4 color;
} render_cmd_block_t;

typedef struct {
        v2 pos;
        v2 size;
        image_t *image;
        b8 flip_x;
} render_cmd_image_t;

typedef struct {
        render_cmd_header_t header;

        union {
                render_cmd_clear_t;
                render_cmd_block_t;
                render_cmd_image_t;
        };
} render_cmd_t;

typedef struct {
        render_cmd_t cmds[4096];
        u32 num_cmds;
        camera_t camera;
} render_cmd_queue_t;

static void draw_block(v2 pos, v2_t size, camera_t *camera,
                       game_frame_buffer_t *frame_buffer, u32 color)
{
        f32 units_to_pixels = camera->units_to_pixels;
        f32 frame_width_units = frame_buffer->width / units_to_pixels;
        f32 frame_height_units = frame_buffer->height / units_to_pixels;
        v2 frame_buffer_half_size = {frame_width_units / 2.0f,
                                          frame_height_units / 2.0f};

        // Calculate top left and bottom right of block.
        v2 half_size = v2_div(size, 2.0f);
        v2 top_left_corner = v2_sub(pos, half_size);
        v2 bot_right_corner = v2_add(pos, half_size);

        // Add draw offsets.
        v2 draw_offset =
            v2_sub(camera->position, frame_buffer_half_size);
        top_left_corner = v2_sub(top_left_corner, draw_offset);
        bot_right_corner = v2_sub(bot_right_corner, draw_offset);

        // Convert to pixel values and round to nearest integer.
        top_left_corner = v2_mul(top_left_corner, units_to_pixels);
        bot_right_corner = v2_mul(bot_right_corner, units_to_pixels);
        vec2i_t top_left_pixel;
        vec2i_t bot_right_pixel;
        v2_floor2(top_left_corner, bot_right_corner, &top_left_pixel,
                     &bot_right_pixel);

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
                        u32 *dest =
                            &frame_buffer->pixels[j + i * frame_buffer->width];
                        *dest = color;
                }
        }
}

void render_block(
