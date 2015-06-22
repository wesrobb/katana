#include "katana_types.h"

static inline void linear_blend(u32 src, u32 *dest)
{
    u8 dest_b = (*dest >> 16) & 0xFF;
    u8 dest_g = (*dest >> 8) & 0xFF;
    u8 dest_r = (*dest >> 0) & 0xFF;

    u8 src_a = (src >> 24) & 0xFF;
    u8 src_b = (src >> 16) & 0xFF;
    u8 src_g = (src >> 8) & 0xFF;
    u8 src_r = (src >> 0) & 0xFF;

    u8 b = ((src_b * src_a) + (dest_b * (255 - src_a))) >> 8;
    u8 g = ((src_g * src_a) + (dest_g * (255 - src_a))) >> 8;
    u8 r = ((src_r * src_a) + (dest_r * (255 - src_a))) >> 8;

    *dest = (u32)b << 16 | (u32)g << 8 | (u32)r;
}

static inline u32 color_to_u32(v4 color)
{
    u32 result = ((u32)(color.r * 255) & 0xFF) << 24 |
                 ((u32)(color.b * 255) & 0xFF) << 16 |
                 ((u32)(color.g * 255) & 0xFF) << 8 |
                 ((u32)(color.a * 255) & 0xFF);
    return result;
}

static void render_clear(render_cmd_clear_t *cmd,
                         game_frame_buffer_t *frame_buffer)
{
    u32 color = color_to_u32(cmd->color);
    u32 *pixel = frame_buffer->pixels;
    for (u32 y = 0; y < frame_buffer->height; ++y) {
        for (u32 x = 0; x < frame_buffer->width; ++x) {
            *pixel = color;
            pixel++;
        }
    }
}

static void render_block(v2 pos, v2 size, v4 color, camera_t *cam,
                         game_frame_buffer_t *frame_buffer)
{
    f32 units_to_pixels = cam->units_to_pixels;
    f32 frame_width_units = frame_buffer->width / units_to_pixels;
    f32 frame_height_units = frame_buffer->height / units_to_pixels;
    v2 frame_buffer_half_size =
        V2(frame_width_units / 2.0f, frame_height_units / 2.0f);

    // Calculate top left and bottom right of block.
    v2 half_size = v2_div(size, 2.0f);
    v2 top_left_corner = v2_sub(pos, half_size);
    v2 bot_right_corner = v2_add(pos, half_size);

    // Add draw offsets.
    v2 draw_offset = v2_sub(cam->position, frame_buffer_half_size);
    top_left_corner = v2_sub(top_left_corner, draw_offset);
    bot_right_corner = v2_sub(bot_right_corner, draw_offset);

    // Convert to pixel values and round to nearest integer.
    top_left_corner = v2_mul(top_left_corner, units_to_pixels);
    bot_right_corner = v2_mul(bot_right_corner, units_to_pixels);
    v2i top_left_pixel;
    v2i bot_right_pixel;
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
    u32 color_u32 = color_to_u32(color);
    for (i32 i = top_left_pixel.y; i < bot_right_pixel.y; ++i) {
        for (i32 j = top_left_pixel.x; j < bot_right_pixel.x; ++j) {
            u32 *dest = &frame_buffer->pixels[j + i * frame_buffer->width];
            *dest = color_u32;
        }
    }
}

static void render_rotated_block(render_cmd_block_t *cmd, camera_t *cam,
                                 game_frame_buffer_t *frame_buffer)
{
    render_basis_t basis = cmd->header.basis;
    v2 size = V2(2, 2);
    v4 color = COLOR(1, 0, 1, 1);
    render_block(basis.origin, size, color, cam, frame_buffer);
    render_block(v2_add(basis.origin, basis.x_axis), size, color, cam,
                 frame_buffer);
    render_block(v2_add(basis.origin, basis.y_axis), size, color, cam,
                 frame_buffer);
    for (u32 y = 0; y < frame_buffer->height; y++) {
        for (u32 x = 0; x < frame_buffer->width; x++) {
        }
    }
}

static void render_image(render_cmd_image_t *cmd, camera_t *cam,
                         game_frame_buffer_t *frame_buffer)
{
    f32 units_to_pixels = cam->units_to_pixels;
    f32 frame_width_units = frame_buffer->width / units_to_pixels;
    f32 frame_height_units = frame_buffer->height / units_to_pixels;
    v2 frame_buffer_half_size =
        V2(frame_width_units / 2.0f, frame_height_units / 2.0f);

    // Resizing ratio.
    v2 actual_size;
    actual_size.x = cmd->image->width / units_to_pixels;
    actual_size.y = cmd->image->height / units_to_pixels;
    f32 x_ratio = actual_size.x / cmd->size.x;
    f32 y_ratio = actual_size.y / cmd->size.y;

    // Calculate top left and bottom right of block.
    v2 half_size = v2_div(cmd->size, 2.0f);
    v2 top_left_corner = v2_sub(cmd->pos, half_size);
    v2 bot_right_corner = v2_add(cmd->pos, half_size);

    // Add draw offsets.
    v2 draw_offset = v2_sub(cam->position, frame_buffer_half_size);
    top_left_corner = v2_sub(top_left_corner, draw_offset);
    bot_right_corner = v2_sub(bot_right_corner, draw_offset);

    // Convert to pixel values and round to nearest integer.
    top_left_corner = v2_mul(top_left_corner, units_to_pixels);
    bot_right_corner = v2_mul(bot_right_corner, units_to_pixels);
    v2i top_left_pixel;
    v2i bot_right_pixel;
    v2_floor2(top_left_corner, bot_right_corner, &top_left_pixel,
              &bot_right_pixel);

    // Bounds checking
    i32 image_data_offset_x = -top_left_pixel.x;
    i32 image_data_offset_y = -top_left_pixel.y;
    i32 flipped_image_data_offset_x = bot_right_pixel.x;
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
    u32 *image_data = (u32 *)cmd->image->data;
    if (cmd->flip_x) {
        for (i32 i = top_left_pixel.y; i < bot_right_pixel.y; ++i) {
            u32 sample_y = (i + image_data_offset_y) * y_ratio;
            for (i32 j = top_left_pixel.x; j < bot_right_pixel.x; ++j) {
                u32 *dest = &frame_buffer->pixels[j + i * frame_buffer->width];
                u32 sample_x = (flipped_image_data_offset_x - j) * x_ratio;
                linear_blend(
                    image_data[sample_x + sample_y * cmd->image->width], dest);
            }
        }
    } else {
        for (i32 i = top_left_pixel.y; i < bot_right_pixel.y; ++i) {
            u32 sample_y = (i + image_data_offset_y) * y_ratio;
            for (i32 j = top_left_pixel.x; j < bot_right_pixel.x; ++j) {
                u32 *dest = &frame_buffer->pixels[j + i * frame_buffer->width];
                u32 sample_x = (j + image_data_offset_x) * x_ratio;
                linear_blend(
                    image_data[sample_x + sample_y * cmd->image->width], dest);
            }
        }
    }
}

void *render_push_cmd(render_queue_t *queue, u32 size)
{
    if (queue->index + size >= queue->size) {
        assert(!"Failed to push render cmd");
        return 0;
    }

    void *result = queue->base + queue->index;
    queue->index += size;
    return result;
}

void render_push_clear(render_queue_t *queue, v4 color)
{
    render_cmd_clear_t *cmd = (render_cmd_clear_t *)render_push_cmd(
        queue, sizeof(render_cmd_clear_t));
    cmd->header.type = render_type_clear;
    cmd->color = color;
}

void render_push_block(render_queue_t *queue, v2 pos, v2 size, v4 color)
{
    render_cmd_block_t *cmd = (render_cmd_block_t *)render_push_cmd(
        queue, sizeof(render_cmd_block_t));
    cmd->header.type = render_type_block;
    cmd->pos = pos;
    cmd->size = size;
    cmd->color = color;
}

void render_push_rotated_block(render_queue_t *queue, render_basis_t *basis,
                               v2 size, v4 color)
{
    render_cmd_block_t *cmd = (render_cmd_block_t *)render_push_cmd(
        queue, sizeof(render_cmd_block_t));
    cmd->header.type = render_type_rotated_block;
    cmd->header.basis = *basis;
    cmd->size = size;
    cmd->color = color;
}

void render_push_image(render_queue_t *queue, v2 pos, v2 size, image_t *image,
                       b8 flip_x)
{
    render_cmd_image_t *cmd = (render_cmd_image_t *)render_push_cmd(
        queue, sizeof(render_cmd_image_t));
    cmd->header.type = render_type_image;
    cmd->pos = pos;
    cmd->size = size;
    cmd->image = image;
    cmd->flip_x = flip_x;
}

render_queue_t *render_alloc_queue(memory_arena_t *arena,
                                   u32 max_render_queue_size, camera_t *cam)
{
    render_queue_t *queue = push_struct(arena, render_queue_t);
    queue->size = max_render_queue_size;
    queue->index = 0;
    queue->base = push_size(arena, max_render_queue_size);
    queue->camera = cam;
    return queue;
}

#include <stdio.h>
void render_draw_queue(render_queue_t *queue, game_frame_buffer_t *frame_buffer)
{
    for (u32 address = 0; address < queue->index;) {
        render_cmd_header_t *header =
            (render_cmd_header_t *)(queue->base + address);
        switch (header->type) {
        case render_type_clear:
            render_clear((render_cmd_clear_t *)header, frame_buffer);
            address += sizeof(render_cmd_clear_t);
            break;
        case render_type_block: {
            render_cmd_block_t *cmd = (render_cmd_block_t *)header;
            render_block(cmd->pos, cmd->size, cmd->color, queue->camera,
                         frame_buffer);
            address += sizeof(render_cmd_block_t);
        } break;
        case render_type_rotated_block:
            render_rotated_block((render_cmd_block_t *)header, queue->camera,
                                 frame_buffer);
            address += sizeof(render_cmd_block_t);
            break;
        case render_type_image:
            render_image((render_cmd_image_t *)header, queue->camera,
                         frame_buffer);
            address += sizeof(render_cmd_image_t);
            break;
        }
    }

    queue->index = 0;
}
