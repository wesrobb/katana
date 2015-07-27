#include "katana_math.h"
#include "katana_types.h"
#include "katana_vec.h"

static inline v4 read_image_color(u32 buffer)
{
    return COLOR(((buffer >> 0) & 0xFF) / 255.0f,
                 ((buffer >> 8) & 0xFF) / 255.0f,
                 ((buffer >> 16) & 0xFF) / 255.0f,
                 ((buffer >> 24) & 0xFF) / 255.0f);
}

static inline v4 read_frame_buffer_color(u32 buffer)
{
    return COLOR(((buffer >> 24) & 0xFF) / 255.0f,
                 ((buffer >> 16) & 0xFF) / 255.0f,
                 ((buffer >> 8) & 0xFF) / 255.0f,
                 ((buffer >> 0) & 0xFF) / 255.0f);
}

static inline u32 color_frame_buffer_u32(v4 color)
{
    u32 result = ((u32)(color.r * 255.0f + 0.5f) & 0xFF) << 24 |
                 ((u32)(color.g * 255.0f + 0.5f) & 0xFF) << 16 |
                 ((u32)(color.b * 255.0f + 0.5f) & 0xFF) << 8 |
                 ((u32)(color.a * 255.0f + 0.5f) & 0xFF);
    return result;
}

static inline u32 color_image_32(v4 color)
{
    u32 result = ((u32)(color.r * 255) & 0xFF) << 0 |
                 ((u32)(color.g * 255) & 0xFF) << 8 |
                 ((u32)(color.b * 255) & 0xFF) << 16 |
                 ((u32)(color.a * 255) & 0xFF) << 24;
    return result;
}

static inline v4 srgb_to_linear(v4 color)
{
    color.r = color.r * color.r;
    color.g = color.g * color.g;
    color.b = color.b * color.b;

    return color;
}

static inline v4 linear_to_srgb(v4 color)
{
    color.r = ksqrt(color.r);
    color.g = ksqrt(color.g);
    color.b = ksqrt(color.b);

    return color;
}

static inline v4 linear_blend(v4 color, v4 tint, v4 dest)
{
    color = v4_hadamard(color, tint);
    f32 a = color.a;
    f32 inv_alpha = 1.0f - a;

    return v4_add(v4_mul(dest, inv_alpha), v4_mul(color, a));
}

// NOTE(Wes): Handle blending of texel from textures directly.
// The switch from u8 to u32 means the texels are read out in
// AA BB GG RR order but must be written to the destination
// pixel as RR BB GG AA.
static inline void linear_blend_texel(u32 texel, u32 *dest)
{
    u8 rs = (u8)((texel >> 0) & 0xFF);
    u8 gs = (u8)((texel >> 8) & 0xFF);
    u8 bs = (u8)((texel >> 16) & 0xFF);
    u8 as = (u8)((texel >> 24) & 0xFF);

    f32 a = as / 255.0f;
    f32 inv_alpha = 1.0f - a;

    u8 rd = (*dest >> 24) & 0xFF;
    u8 gd = (*dest >> 16) & 0xFF;
    u8 bd = (*dest >> 8) & 0xFF;
    u8 ad = (*dest >> 0) & 0xFF;

    u8 r = (u8)(rs + 0.5f) + (u8)(rd * inv_alpha + 0.5f);
    u8 g = (u8)(gs + 0.5f) + (u8)(gd * inv_alpha + 0.5f);
    u8 b = (u8)(bs + 0.5f) + (u8)(bd * inv_alpha + 0.5f);

    u32 temp = r << 24 | g << 16 | b << 8 | ad << 0;

    *dest = temp;
}

static void render_clear(render_cmd_clear_t *cmd,
                         game_frame_buffer_t *frame_buffer)
{
    for (u32 y = 0; y < frame_buffer->h; ++y) {
        u32 *pixels = frame_buffer->data + y * frame_buffer->pitch;
        for (u32 x = 0; x < frame_buffer->w; ++x) {
            pixels[x] = (u8)(cmd->color.r * 255.0f + 0.5f) << 24 |
                        (u8)(cmd->color.g * 255.0f + 0.5f) << 16 |
                        (u8)(cmd->color.b * 255.0f + 0.5f) << 8 |
                        (u8)(cmd->color.a * 255.0f + 0.5f) << 0;
        }
    }
}

static void render_block(v2 pos,
                         v2 size,
                         v4 color,
                         camera_t *cam,
                         game_frame_buffer_t *frame_buffer)
{
    f32 units_to_pixels = cam->units_to_pixels;
    f32 frame_width_units = frame_buffer->w / units_to_pixels;
    f32 frame_height_units = frame_buffer->h / units_to_pixels;
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
    v2_floor2(
        top_left_corner, bot_right_corner, &top_left_pixel, &bot_right_pixel);

    // Bounds checking
    if (top_left_pixel.x < 0) {
        top_left_pixel.x = 0;
    } else if (top_left_pixel.x > (i32)frame_buffer->w) {
        top_left_pixel.x = frame_buffer->w;
    }
    if (top_left_pixel.y < 0) {
        top_left_pixel.y = 0;
    } else if (top_left_pixel.y > (i32)frame_buffer->h) {
        top_left_pixel.y = frame_buffer->h;
    }
    if (bot_right_pixel.x < 0) {
        bot_right_pixel.x = 0;
    } else if (bot_right_pixel.x > (i32)frame_buffer->w) {
        bot_right_pixel.x = frame_buffer->w;
    }
    if (bot_right_pixel.y < 0) {
        bot_right_pixel.y = 0;
    } else if (bot_right_pixel.y > (i32)frame_buffer->h) {
        bot_right_pixel.y = frame_buffer->h;
    }

    for (i32 i = top_left_pixel.y; i < bot_right_pixel.y; ++i) {
        u32 *pixels = frame_buffer->data + i * frame_buffer->pitch;
        for (i32 j = top_left_pixel.x; j < bot_right_pixel.x; ++j) {
            pixels[j] = (u8)(color.r * 255.0f + 0.5f) << 24 |
                        (u8)(color.g * 255.0f + 0.5f) << 16 |
                        (u8)(color.b * 255.0f + 0.5f) << 8 |
                        (u8)(color.a * 255.0f + 0.5f) << 0;
        }
    }
}

static void render_rotated_block(render_cmd_block_t *cmd,
                                 camera_t *cam,
                                 game_frame_buffer_t *frame_buffer)
{
    render_basis_t basis = cmd->header.basis;

    v2 origin = v2_mul(basis.origin, cam->units_to_pixels);
    v2 x_axis = v2_mul(basis.x_axis, cam->units_to_pixels);
    v2 y_axis = v2_mul(basis.y_axis, cam->units_to_pixels);

    i32 max_width = frame_buffer->w - 1;
    i32 max_height = frame_buffer->h - 1;
    i32 x_min = max_width;
    i32 x_max = 0;
    i32 y_min = max_height;
    i32 y_max = 0;

    // NOTE(Wes): Find the max rect we could possible draw in regardless
    // of rotation.
    v2i floor_bounds[4];
    v2_floor2(
        origin, v2_add(origin, x_axis), &floor_bounds[0], &floor_bounds[1]);
    v2_floor2(v2_add(origin, y_axis),
              v2_add(v2_add(origin, y_axis), x_axis),
              &floor_bounds[2],
              &floor_bounds[3]);

    v2i ceil_bounds[4];
    v2_ceil2(origin, v2_add(origin, x_axis), &ceil_bounds[0], &ceil_bounds[1]);
    v2_ceil2(v2_add(origin, y_axis),
             v2_add(v2_add(origin, y_axis), x_axis),
             &ceil_bounds[2],
             &ceil_bounds[3]);

    for (u32 i = 0; i < 4; ++i) {
        v2i floor_bound = floor_bounds[i];
        v2i ceil_bound = ceil_bounds[i];
        if (x_min > floor_bound.x) {
            x_min = floor_bound.x;
        }
        if (y_min > floor_bound.y) {
            y_min = floor_bound.y;
        }
        if (x_max < ceil_bound.x) {
            x_max = ceil_bound.x;
        }
        if (y_max < ceil_bound.y) {
            y_max = ceil_bound.y;
        }
    }

    // NOTE(Wes): Clip to frame_buffer edges.
    if (x_min < 0) {
        x_min = 0;
    }
    if (x_max > max_width) {
        x_max = max_width;
    }
    if (y_min < 0) {
        y_min = 0;
    }
    if (y_max < max_height) {
        y_max = max_height;
    }

    f32 inv_x_len_sq = 1.0f / v2_len_sq(x_axis);
    f32 inv_y_len_sq = 1.0f / v2_len_sq(y_axis);

    image_t *texture = cmd->image;
    v4 tint = cmd->tint;

    u32 *fb_data = frame_buffer->data;
    for (i32 y = y_min; y <= y_max; y++) {
        for (i32 x = x_min; x <= x_max; x++) {
            // NOTE(Wes): Take the dot product of the point and the axis
            // perpendicular to the one we are testing to see which side
            // the point lies on. This code moves the point p in an
            // anti-clockwise direction.
            v2 p_orig = v2_sub(V2(x, y), origin);
            v2 p = v2_sub(V2(x, y), origin);
            f32 edge1 = v2_dot(p, v2_neg(y_axis));
            p = v2_sub(p, x_axis);
            f32 edge2 = v2_dot(p, x_axis);
            p = v2_sub(p, y_axis);
            f32 edge3 = v2_dot(p, y_axis);
            p = v2_add(p, x_axis);
            f32 edge4 = v2_dot(p, v2_neg(x_axis));
            if (edge1 < 0 && edge2 < 0 && edge3 < 0 && edge4 < 0) {
                f32 u = inv_x_len_sq * v2_dot(p_orig, x_axis);
                f32 v = inv_y_len_sq * v2_dot(p_orig, y_axis);

                // NOTE(Wes): Clamp uv for float rounding errors.
                u = kclampf(u, 0.0f, 1.0f);
                v = kclampf(v, 0.0f, 1.0f);

                // TODO(Wes): Actually clamp the texels for subpixel rendering
                // rather than just shortening the texture.
                f32 texel_x = 1.0f + (u * (f32)(texture->w - 3)) + 0.5f;
                f32 texel_y = 1.0f + (v * (f32)(texture->h - 3)) + 0.5f;

                u32 texture_x = (u32)texel_x;
                u32 texture_y = (u32)texel_y;

                // NOTE(Wes): Get the rounded off value of the texel.
                f32 fraction_x = texel_x - texture_x;
                f32 fraction_y = texel_y - texture_y;

                texture_x = kclamp(texture_x, 0, texture->w - 1);
                texture_y = kclamp(texture_y, 0, texture->h - 1);

                u32 *texel = &texture->data[texture_y * texture->w + texture_x];

                // NOTE(Wes): Blend the closest 4 texels for better looking
                // pixels. Bilinear blending.
                v4 texel_a = read_image_color(*texel);
                v4 texel_b = read_image_color(*(texel + 1));
                v4 texel_c = read_image_color(*(texel + texture->w));
                v4 texel_d = read_image_color(*(texel + texture->w + 1));

                // NOTE(Wes): Perform gamma correction on pixels
                // before blending them to ensure all math is done
                // in linear space.
                texel_a = srgb_to_linear(texel_a);
                texel_b = srgb_to_linear(texel_b);
                texel_c = srgb_to_linear(texel_c);
                texel_d = srgb_to_linear(texel_d);

                v4 blended_color =
                    v4_lerp(v4_lerp(texel_a, texel_b, fraction_x),
                            v4_lerp(texel_c, texel_d, fraction_x),
                            fraction_y);

                u32 *fb_pixel = &fb_data[x + y * frame_buffer->w];
                v4 dest_color = read_frame_buffer_color(*fb_pixel);

                dest_color = srgb_to_linear(dest_color);
                dest_color = linear_blend(blended_color, tint, dest_color);
                dest_color = linear_to_srgb(dest_color);

                *fb_pixel = color_frame_buffer_u32(dest_color);
            }
        }
    }
}

static void render_image(render_cmd_image_t *cmd,
                         camera_t *cam,
                         game_frame_buffer_t *frame_buffer)
{
    f32 units_to_pixels = cam->units_to_pixels;
    f32 frame_width_units = frame_buffer->w / units_to_pixels;
    f32 frame_height_units = frame_buffer->h / units_to_pixels;
    v2 frame_buffer_half_size =
        V2(frame_width_units / 2.0f, frame_height_units / 2.0f);

    // Resizing ratio.
    v2 actual_size;
    actual_size.x = cmd->image->w / units_to_pixels;
    actual_size.y = cmd->image->h / units_to_pixels;
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
    v2_floor2(
        top_left_corner, bot_right_corner, &top_left_pixel, &bot_right_pixel);

    // Bounds checking
    i32 image_data_offset_x = -top_left_pixel.x;
    i32 image_data_offset_y = -top_left_pixel.y;
    i32 flipped_image_data_offset_x = bot_right_pixel.x;
    if (top_left_pixel.x < 0) {
        top_left_pixel.x = 0;
    } else if (top_left_pixel.x > (i32)frame_buffer->w) {
        top_left_pixel.x = frame_buffer->w;
    }
    if (top_left_pixel.y < 0) {
        top_left_pixel.y = 0;
    } else if (top_left_pixel.y > (i32)frame_buffer->h) {
        top_left_pixel.y = frame_buffer->h;
    }
    if (bot_right_pixel.x < 0) {
        bot_right_pixel.x = 0;
    } else if (bot_right_pixel.x > (i32)frame_buffer->w) {
        bot_right_pixel.x = frame_buffer->w;
    }
    if (bot_right_pixel.y < 0) {
        bot_right_pixel.y = 0;
    } else if (bot_right_pixel.y > (i32)frame_buffer->h) {
        bot_right_pixel.y = frame_buffer->h;
    }
    if (cmd->flip_x) {
        for (i32 i = top_left_pixel.y; i < bot_right_pixel.y; ++i) {
            u32 sample_y = (i + image_data_offset_y) * y_ratio;
            for (i32 j = top_left_pixel.x; j < bot_right_pixel.x; ++j) {
                u32 sample_x = (flipped_image_data_offset_x - j) * x_ratio;
                u32 color =
                    cmd->image->data[sample_x + sample_y * cmd->image->w];
                u32 *dest = &frame_buffer->data[j + i * frame_buffer->w];
                linear_blend_texel(color, dest);
            }
        }
    } else {
        for (i32 i = top_left_pixel.y; i < bot_right_pixel.y; ++i) {
            u32 sample_y = (i + image_data_offset_y) * y_ratio;
            for (i32 j = top_left_pixel.x; j < bot_right_pixel.x; ++j) {
                u32 sample_x = (j + image_data_offset_x) * x_ratio;
                u32 color =
                    cmd->image->data[sample_x + sample_y * cmd->image->w];
                u32 *dest = &frame_buffer->data[j + i * frame_buffer->w];
                linear_blend_texel(color, dest);
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

void render_push_block(render_queue_t *queue, v2 pos, v2 size, v4 tint)
{
    render_cmd_block_t *cmd = (render_cmd_block_t *)render_push_cmd(
        queue, sizeof(render_cmd_block_t));
    cmd->header.type = render_type_block;
    cmd->pos = pos;
    cmd->size = size;
    cmd->tint = tint;
}

void render_push_rotated_block(render_queue_t *queue,
                               render_basis_t *basis,
                               v2 size,
                               v4 tint,
                               image_t *image)
{
    render_cmd_block_t *cmd = (render_cmd_block_t *)render_push_cmd(
        queue, sizeof(render_cmd_block_t));
    cmd->header.type = render_type_rotated_block;
    cmd->header.basis = *basis;
    cmd->size = size;
    cmd->tint = tint;
    cmd->image = image;
}

void render_push_image(render_queue_t *queue,
                       v2 pos,
                       v2 size,
                       image_t *image,
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
                                   u32 max_render_queue_size,
                                   camera_t *cam)
{
    render_queue_t *queue = push_struct(arena, render_queue_t);
    queue->size = max_render_queue_size;
    queue->index = 0;
    queue->base = push_size(arena, max_render_queue_size);
    queue->camera = cam;
    return queue;
}

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
            render_block(
                cmd->pos, cmd->size, cmd->tint, queue->camera, frame_buffer);
            address += sizeof(render_cmd_block_t);
        } break;
        case render_type_rotated_block:
            render_rotated_block(
                (render_cmd_block_t *)header, queue->camera, frame_buffer);
            address += sizeof(render_cmd_block_t);
            break;
        case render_type_image:
            render_image(
                (render_cmd_image_t *)header, queue->camera, frame_buffer);
            address += sizeof(render_cmd_image_t);
            break;
        }
    }

    queue->index = 0;
}
