#include "gg_types.h"
#include "gg_aabb.h"
#include "gg_math.h"
#include "gg_vec.h"

#include "xmmintrin.h"

typedef enum {
    render_type_clear,
    render_type_image,
    render_type_rect
} render_type_t;

typedef struct {
    render_type_t type;
    basis_t basis;
} render_cmd_header_t;

typedef struct {
    render_cmd_header_t header;
    v4 color;
} render_cmd_clear_t;

typedef struct {
    render_cmd_header_t header;
    v2 pos;
    v2 size;
    v4 tint;
    f32 rotation; // Radians
    image_t *image;
    image_t *normals;
    light_t *lights;
    u32 num_lights;
} render_cmd_image_t;

typedef struct {
    render_cmd_header_t header;
    v4 color;
    f32 border_size;
} render_cmd_rect_t;

static inline v4 read_frame_buffer_color(u32 buffer)
{
    return COLOR(((buffer >> 24) & 0xFF) / 255.0f,
                 ((buffer >> 16) & 0xFF) / 255.0f,
                 ((buffer >> 8) & 0xFF) / 255.0f,
                 ((buffer >> 0) & 0xFF) / 255.0f);
}

static inline u32 color_frame_buffer_u32(v4 color)
{
    u32 result = ((u32)(color.a * 255.0f + 0.5f) & 0xFF) << 24 | ((u32)(color.r * 255.0f + 0.5f) & 0xFF) << 16 |
                 ((u32)(color.g * 255.0f + 0.5f) & 0xFF) << 8 | ((u32)(color.b * 255.0f + 0.5f) & 0xFF);
    return result;
}

u32 color_image_32(v4 color)
{
    u32 result = ((u32)(color.a * 255) & 0xFF) << 0 | ((u32)(color.r * 255) & 0xFF) << 8 |
                 ((u32)(color.g * 255) & 0xFF) << 16 | ((u32)(color.b * 255) & 0xFF) << 24;
    return result;
}

v4 read_image_color(u32 buffer)
{
    return COLOR(((buffer >> 0) & 0xff) / 255.0f,
                 ((buffer >> 8) & 0xff) / 255.0f,
                 ((buffer >> 16) & 0xff) / 255.0f,
                 ((buffer >> 24) & 0xff) / 255.0f);
}

v4 srgb_to_linear(v4 color)
{
    color.r = color.r * color.r;
    color.g = color.g * color.g;
    color.b = color.b * color.b;

    return color;
}

v4 linear_to_srgb(v4 color)
{
    color.r = ksqrt(color.r);
    color.g = ksqrt(color.g);
    color.b = ksqrt(color.b);

    return color;
}

static inline v4 linear_blend(v4 color, v4 dest)
{
    f32 a = color.a;
    f32 inv_alpha = 1.0f - a;

    return v4_add(v4_mul(dest, inv_alpha), v4_mul(color, a));
}

static inline v4 linear_blend_tint(v4 color, v4 tint, v4 dest)
{
    color = v4_hadamard(color, tint);
    return linear_blend(color, dest);
}

// NOTE(Wes): Handle blending of texel from textures directly.
// The switch from u8 to u32 means the texels are read out in
// AA BB GG RR order but must be written to the destination
// pixel as RR BB GG AA.
static inline void linear_blend_texel(u32 texel, u32 *dest)
{
    u8 as = (u8)((texel >> 0) & 0xFF);
    u8 rs = (u8)((texel >> 8) & 0xFF);
    u8 gs = (u8)((texel >> 16) & 0xFF);
    u8 bs = (u8)((texel >> 24) & 0xFF);

    f32 a = as / 255.0f;
    f32 inv_alpha = 1.0f - a;

    u8 ad = (*dest >> 24) & 0xFF;
    u8 rd = (*dest >> 16) & 0xFF;
    u8 gd = (*dest >> 8) & 0xFF;
    u8 bd = (*dest >> 0) & 0xFF;

    u8 r = (u8)(rs + 0.5f) + (u8)(rd * inv_alpha + 0.5f);
    u8 g = (u8)(gs + 0.5f) + (u8)(gd * inv_alpha + 0.5f);
    u8 b = (u8)(bs + 0.5f) + (u8)(bd * inv_alpha + 0.5f);

    u32 temp = ad << 24 | r << 16 | g << 8 | b << 0;

    *dest = temp;
}

static void render_clear(render_cmd_clear_t *cmd, game_frame_buffer_t *frame_buffer, aabb2i_t clip_rect)
{
    for (i32 y = clip_rect.y_min; y < clip_rect.y_max; ++y) {
        u32 *pixels = (u32 *)(frame_buffer->data + y * frame_buffer->pitch);
        for (i32 x = clip_rect.x_min; x < clip_rect.x_max; ++x) {
            pixels[x] = (u8)(cmd->color.a * 255.0f + 0.5f) << 24 | (u8)(cmd->color.r * 255.0f + 0.5f) << 16 |
                        (u8)(cmd->color.g * 255.0f + 0.5f) << 8 | (u8)(cmd->color.b * 255.0f + 0.5f) << 0;
        }
    }
}

#define m128i_access(a, i) ((u32 *)&a)[i]
#define m128_access(a, i) ((f32 *)&a)[i]
#define m128_access8(a, i) ((u8 *)&a)[i]

#ifdef GG_DEBUG
typedef struct {
    u8 a, b, g, r;
} pixel_t;
#endif

static void render_image(render_cmd_image_t *cmd, camera_t *cam, game_frame_buffer_t *frame_buffer, aabb2i_t clip_rect)
{
    START_COUNTER(render_image);
    basis_t basis = cmd->header.basis;

    v2 origin = v2_mul(basis.origin, cam->units_to_pixels);
    v2 x_axis = v2_mul(basis.x_axis, cam->units_to_pixels);
    v2 y_axis = v2_mul(basis.y_axis, cam->units_to_pixels);

    i32 max_width = frame_buffer->w - 1;
    i32 max_height = frame_buffer->h - 1;
    aabb2i_t fill_rect = aabb2i_inverted_infinity();

    // NOTE(Wes): Find the max rect we could possible draw in regardless
    // of rotation.
    v2i floor_bounds[4];
    floor_bounds[0] = v2_floor(origin);
    floor_bounds[1] = v2_floor(v2_add(origin, x_axis));
    floor_bounds[2] = v2_floor(v2_add(origin, y_axis));
    floor_bounds[3] = v2_floor(v2_add(v2_add(origin, y_axis), x_axis));

    v2i ceil_bounds[4];
    ceil_bounds[0] = v2_ceil(origin);
    ceil_bounds[1] = v2_ceil(v2_add(origin, x_axis));
    ceil_bounds[2] = v2_ceil(v2_add(origin, y_axis));
    ceil_bounds[3] = v2_ceil(v2_add(v2_add(origin, y_axis), x_axis));

    for (u32 i = 0; i < 4; ++i) {
        v2i floor_bound = floor_bounds[i];
        v2i ceil_bound = ceil_bounds[i];
        if (fill_rect.x_min > floor_bound.x) {
            fill_rect.x_min = floor_bound.x;
        }
        if (fill_rect.y_min > floor_bound.y) {
            fill_rect.y_min = floor_bound.y;
        }
        if (fill_rect.x_max < ceil_bound.x) {
            fill_rect.x_max = ceil_bound.x;
        }
        if (fill_rect.y_max < ceil_bound.y) {
            fill_rect.y_max = ceil_bound.y;
        }
    }

#if 1
    // NOTE(Wes): Clip to clip rect edges.
    fill_rect = aabb2i_intersect(fill_rect, clip_rect);

    if (!aabb2i_has_area(fill_rect)) {
        END_COUNTER(render_image);
        return;
    }
#else
    if (!aabb2i_has_area(fill_rect)) {
        END_COUNTER(render_image);
        return;
    }

    // NOTE(Wes): Clip to clip rect edges.
    fill_rect = aabb2i_intersect(fill_rect, clip_rect);
#endif

    // Align the 4px write to x_max and write mask overwrite on x_min boundary.
    __m128i start_clip_mask = _mm_set1_epi32(0xFFFFFFFF);
    __m128i end_clip_mask = _mm_set1_epi32(0xFFFFFFFF);
    __m128i start_clip_masks[3];
    start_clip_masks[0] = _mm_setr_epi32(0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
    start_clip_masks[1] = _mm_setr_epi32(0, 0, 0xFFFFFFFF, 0xFFFFFFFF);
    start_clip_masks[2] = _mm_setr_epi32(0, 0, 0, 0xFFFFFFFF);
    __m128i end_clip_masks[3];
    end_clip_masks[0] = _mm_setr_epi32(0xFFFFFFFF, 0, 0, 0);
    end_clip_masks[1] = _mm_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0, 0);
    end_clip_masks[2] = _mm_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0);

    // Align fill x values to 4 pixel boundaries
    // and mask off pixels that should not be drawn
    i32 x_min_align = fill_rect.x_min & 3;
    if (x_min_align) {
        fill_rect.x_min = fill_rect.x_min & ~3;
        start_clip_mask = start_clip_masks[x_min_align - 1];
    }

    i32 x_max_align = fill_rect.x_max & 3;
    if (x_max_align) {
        fill_rect.x_max = (fill_rect.x_max & ~3) + 4;
        end_clip_mask = end_clip_masks[x_max_align - 1];
    }


    f32 inv_x_len_sq = 1.0f / v2_len_sq(x_axis);
    f32 inv_y_len_sq = 1.0f / v2_len_sq(y_axis);
    v2 n_x_axis = v2_mul(x_axis, inv_x_len_sq);
    v2 n_y_axis = v2_mul(y_axis, inv_y_len_sq);
    __m128 n_x_axis_x4 = _mm_set1_ps(n_x_axis.x);
    __m128 n_x_axis_y4 = _mm_set1_ps(n_x_axis.y);
    __m128 n_y_axis_x4 = _mm_set1_ps(n_y_axis.x);
    __m128 n_y_axis_y4 = _mm_set1_ps(n_y_axis.y);

    image_t *texture = cmd->image;
    u32 *texture_data = texture->data;
    u32 texture_width = texture->w;
    u32 texture_height = texture->h;
    __m128i texture_width4 = _mm_set1_epi32(texture_width);
    __m128i texture_height4 = _mm_set1_epi32(texture_height);
    __m128 texture_width4f = _mm_cvtepi32_ps(texture_width4);
    __m128 texture_height4f = _mm_cvtepi32_ps(texture_height4);
    image_t *normals = cmd->normals;

    __m128 zero4 = _mm_setzero_ps();
    __m128 one4 = _mm_set1_ps(1.0f);
    __m128i zero4i = _mm_setzero_si128();
    __m128i one4i = _mm_set1_epi32(1);

    __m128i texel_mask = _mm_set1_epi32(0x000000FF);
    __m128 inv_255 = _mm_set1_ps(1.0f / 255.0f);
    __m128 two_fifty_five4 = _mm_set1_ps(255.0f);

    __m128i first_mask = _mm_setr_epi32(0xFFFFFFFF, 0, 0, 0);
    __m128i second_mask = _mm_setr_epi32(0, 0xFFFFFFFF, 0, 0);
    __m128i third_mask = _mm_setr_epi32(0, 0, 0xFFFFFFFF, 0);
    __m128i fourth_mask = _mm_setr_epi32(0, 0, 0, 0xFFFFFFFF);

    u8 *fb_data = frame_buffer->data;
    START_COUNTER(process_pixel);
    for (i32 y = fill_rect.y_min; y < fill_rect.y_max; y++) {
        __m128 p_orig_y4 = _mm_set1_ps(y - origin.y);
        __m128i clip_mask = start_clip_mask;

        for (i32 x = fill_rect.x_min; x < fill_rect.x_max; x += 4) {
            // Calculate pixel origin
            __m128 p_orig_x4 = _mm_setr_ps(x - origin.x, x - origin.x + 1, x - origin.x + 2, x - origin.x + 3);

            // Calculate texture coord
            __m128 u4 = _mm_add_ps(_mm_mul_ps(p_orig_x4, n_x_axis_x4), _mm_mul_ps(p_orig_y4, n_x_axis_y4));
            __m128 v4 = _mm_add_ps(_mm_mul_ps(p_orig_x4, n_y_axis_x4), _mm_mul_ps(p_orig_y4, n_y_axis_y4));

            // Ensure texture coord is within bounds. Save result to mask.
            __m128 u4_ge_zero = _mm_cmpge_ps(u4, zero4);
            __m128 u4_le_one = _mm_cmple_ps(u4, one4);
            __m128 v4_ge_zero = _mm_cmpge_ps(v4, zero4);
            __m128 v4_le_one = _mm_cmple_ps(v4, one4);
            __m128i write_mask =
                _mm_castps_si128(_mm_and_ps(_mm_and_ps(u4_ge_zero, u4_le_one), _mm_and_ps(v4_ge_zero, v4_le_one)));

            write_mask = _mm_and_si128(write_mask, clip_mask);

#if 0
            if (m128i_access(write_mask, 0) == 0 && m128i_access(write_mask, 1) == 0 &&
                m128i_access(write_mask, 2) == 0 && m128i_access(write_mask, 3) == 0) {
                continue;
            }
#endif

            // Clamp texture coord
            u4 = _mm_max_ps(_mm_min_ps(u4, one4), zero4);
            v4 = _mm_max_ps(_mm_min_ps(v4, one4), zero4);

            // Calculate the actual texel position in the texture
            // TODO(Wes): Formalize texture boundaries
            __m128 texture_width4fm1 = _mm_sub_ps(texture_width4f, one4);
            __m128 texture_height4fm1 = _mm_sub_ps(texture_height4f, one4);
            __m128 texel_x4 = _mm_mul_ps(u4, texture_width4fm1);
            __m128 texel_y4 = _mm_mul_ps(v4, texture_height4fm1);

            // Calculate the blend factor for surrounding pixels
            __m128i texture_x4 = _mm_cvttps_epi32(texel_x4);
            __m128i texture_y4 = _mm_cvttps_epi32(texel_y4);
            __m128 subpixel_x4 = _mm_sub_ps(texel_x4, _mm_cvtepi32_ps(texture_x4));
            __m128 subpixel_y4 = _mm_sub_ps(texel_y4, _mm_cvtepi32_ps(texture_y4));
            __m128 inv_subpixel_x4 = _mm_sub_ps(one4, subpixel_x4);
            __m128 inv_subpixel_y4 = _mm_sub_ps(one4, subpixel_y4);

            texture_x4 = _mm_max_epi32(_mm_min_epi32(texture_x4, _mm_sub_epi32(texture_width4, one4i)), zero4i);
            texture_y4 = _mm_max_epi32(_mm_min_epi32(texture_y4, _mm_sub_epi32(texture_height4, one4i)), zero4i);

            // TODO(Wes): This is SSE4. We need to find a way to do this mul in SSE2.
            __m128i texture_index = _mm_add_epi32(texture_x4, _mm_mullo_epi32(texture_width4, texture_y4));

            u32 texture_index0 = m128i_access(texture_index, 0);
            u32 texture_index1 = m128i_access(texture_index, 1);
            u32 texture_index2 = m128i_access(texture_index, 2);
            u32 texture_index3 = m128i_access(texture_index, 3);
            u32 *base_addr1 = &texture_data[texture_index0];
            u32 *base_addr2 = &texture_data[texture_index1];
            u32 *base_addr3 = &texture_data[texture_index2];
            u32 *base_addr4 = &texture_data[texture_index3];

            __m128i sample_a = _mm_setr_epi32(*base_addr1, *base_addr2, *base_addr3, *base_addr4);
            __m128i sample_b =
                _mm_setr_epi32(*(base_addr1 + 1), *(base_addr2 + 1), *(base_addr3 + 1), *(base_addr4 + 1));
            __m128i sample_c = _mm_setr_epi32(*(base_addr1 + texture_width),
                                              *(base_addr2 + texture_width),
                                              *(base_addr3 + texture_width),
                                              *(base_addr4 + texture_width));
            __m128i sample_d = _mm_setr_epi32(*(base_addr1 + texture_width + 1),
                                              *(base_addr2 + texture_width + 1),
                                              *(base_addr3 + texture_width + 1),
                                              *(base_addr4 + texture_width + 1));

            // Pack the 4 samples into 4 texels (rrrr, gggg, bbbb, aaaa)
            __m128i texel_a_a4i = _mm_and_si128(texel_mask, sample_a);
            __m128i texel_a_r4i = _mm_and_si128(texel_mask, _mm_srli_epi32(sample_a, 8));
            __m128i texel_a_g4i = _mm_and_si128(texel_mask, _mm_srli_epi32(sample_a, 16));
            __m128i texel_a_b4i = _mm_and_si128(texel_mask, _mm_srli_epi32(sample_a, 24));

            __m128i texel_b_a4i = _mm_and_si128(texel_mask, sample_b);
            __m128i texel_b_r4i = _mm_and_si128(texel_mask, _mm_srli_epi32(sample_b, 8));
            __m128i texel_b_g4i = _mm_and_si128(texel_mask, _mm_srli_epi32(sample_b, 16));
            __m128i texel_b_b4i = _mm_and_si128(texel_mask, _mm_srli_epi32(sample_b, 24));

            __m128i texel_c_a4i = _mm_and_si128(texel_mask, sample_c);
            __m128i texel_c_r4i = _mm_and_si128(texel_mask, _mm_srli_epi32(sample_c, 8));
            __m128i texel_c_g4i = _mm_and_si128(texel_mask, _mm_srli_epi32(sample_c, 16));
            __m128i texel_c_b4i = _mm_and_si128(texel_mask, _mm_srli_epi32(sample_c, 24));

            __m128i texel_d_a4i = _mm_and_si128(texel_mask, sample_d);
            __m128i texel_d_r4i = _mm_and_si128(texel_mask, _mm_srli_epi32(sample_d, 8));
            __m128i texel_d_g4i = _mm_and_si128(texel_mask, _mm_srli_epi32(sample_d, 16));
            __m128i texel_d_b4i = _mm_and_si128(texel_mask, _mm_srli_epi32(sample_d, 24));

            // Convert packed texels from u8 in range 0-255 to f32 in range 0-1.
            __m128 texel_a_a4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_a_a4i), inv_255);
            __m128 texel_a_r4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_a_r4i), inv_255);
            __m128 texel_a_g4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_a_g4i), inv_255);
            __m128 texel_a_b4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_a_b4i), inv_255);

            __m128 texel_b_a4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_b_a4i), inv_255);
            __m128 texel_b_r4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_b_r4i), inv_255);
            __m128 texel_b_g4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_b_g4i), inv_255);
            __m128 texel_b_b4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_b_b4i), inv_255);

            __m128 texel_c_a4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_c_a4i), inv_255);
            __m128 texel_c_r4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_c_r4i), inv_255);
            __m128 texel_c_g4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_c_g4i), inv_255);
            __m128 texel_c_b4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_c_b4i), inv_255);

            __m128 texel_d_a4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_d_a4i), inv_255);
            __m128 texel_d_r4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_d_r4i), inv_255);
            __m128 texel_d_g4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_d_g4i), inv_255);
            __m128 texel_d_b4 = _mm_mul_ps(_mm_cvtepi32_ps(texel_d_b4i), inv_255);

            // Optimized linear blend
            __m128 c0 = _mm_mul_ps(subpixel_x4, subpixel_y4);
            __m128 c1 = _mm_mul_ps(inv_subpixel_x4, subpixel_y4);
            __m128 c2 = _mm_mul_ps(subpixel_x4, inv_subpixel_y4);
            __m128 c3 = _mm_mul_ps(inv_subpixel_x4, inv_subpixel_y4);
            __m128 blended_a4 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(texel_d_a4, c0), _mm_mul_ps(texel_c_a4, c1)),
                                           _mm_add_ps(_mm_mul_ps(texel_b_a4, c2), _mm_mul_ps(texel_a_a4, c3)));
            __m128 blended_r4 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(texel_d_r4, c0), _mm_mul_ps(texel_c_r4, c1)),
                                           _mm_add_ps(_mm_mul_ps(texel_b_r4, c2), _mm_mul_ps(texel_a_r4, c3)));
            __m128 blended_g4 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(texel_d_g4, c0), _mm_mul_ps(texel_c_g4, c1)),
                                           _mm_add_ps(_mm_mul_ps(texel_b_g4, c2), _mm_mul_ps(texel_a_g4, c3)));
            __m128 blended_b4 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(texel_d_b4, c0), _mm_mul_ps(texel_c_b4, c1)),
                                           _mm_add_ps(_mm_mul_ps(texel_b_b4, c2), _mm_mul_ps(texel_a_b4, c3)));

            __m128i *fb_pixel = (__m128i *)&fb_data[(x * GG_BYTES_PP) + y * frame_buffer->pitch];
            __m128i fb_pixel_packed = _mm_load_si128(fb_pixel);

            // Convert current value in framebuffer to rrrr, gggg, bbbb, aaaa
            __m128i fb_pixel_b4i = _mm_and_si128(texel_mask, fb_pixel_packed);
            __m128i fb_pixel_g4i = _mm_and_si128(texel_mask, _mm_srli_epi32(fb_pixel_packed, 8));
            __m128i fb_pixel_r4i = _mm_and_si128(texel_mask, _mm_srli_epi32(fb_pixel_packed, 16));
            __m128i fb_pixel_a4i = _mm_and_si128(texel_mask, _mm_srli_epi32(fb_pixel_packed, 24));

            // Convert packed frame buffer values from u8 (0-255) to f32 (0.0f-1.0f)
            __m128 fb_pixel_b4 = _mm_mul_ps(_mm_cvtepi32_ps(fb_pixel_b4i), inv_255);
            __m128 fb_pixel_g4 = _mm_mul_ps(_mm_cvtepi32_ps(fb_pixel_g4i), inv_255);
            __m128 fb_pixel_r4 = _mm_mul_ps(_mm_cvtepi32_ps(fb_pixel_r4i), inv_255);
            __m128 fb_pixel_a4 = _mm_mul_ps(_mm_cvtepi32_ps(fb_pixel_a4i), inv_255);

            __m128 inv_alpha4 = _mm_sub_ps(one4, blended_a4);
            fb_pixel_r4 = _mm_add_ps(_mm_mul_ps(fb_pixel_r4, inv_alpha4), _mm_mul_ps(blended_r4, blended_a4));
            fb_pixel_g4 = _mm_add_ps(_mm_mul_ps(fb_pixel_g4, inv_alpha4), _mm_mul_ps(blended_g4, blended_a4));
            fb_pixel_b4 = _mm_add_ps(_mm_mul_ps(fb_pixel_b4, inv_alpha4), _mm_mul_ps(blended_b4, blended_a4));
            fb_pixel_a4 = _mm_add_ps(_mm_mul_ps(fb_pixel_a4, inv_alpha4), _mm_mul_ps(blended_a4, blended_a4));

            fb_pixel_a4i = _mm_cvtps_epi32(_mm_mul_ps(fb_pixel_a4, two_fifty_five4));
            fb_pixel_b4i = _mm_cvtps_epi32(_mm_mul_ps(fb_pixel_b4, two_fifty_five4));
            fb_pixel_g4i = _mm_cvtps_epi32(_mm_mul_ps(fb_pixel_g4, two_fifty_five4));
            fb_pixel_r4i = _mm_cvtps_epi32(_mm_mul_ps(fb_pixel_r4, two_fifty_five4));

            // Repack fb_pixels from  aaaa, rrrr, gggg, bbbb to arbg, argb, argb, argb
            __m128i temp_br_lo = _mm_unpacklo_epi32(fb_pixel_b4i, fb_pixel_r4i);
            __m128i temp_br_hi = _mm_unpackhi_epi32(fb_pixel_b4i, fb_pixel_r4i);
            __m128i temp_ga_lo = _mm_unpacklo_epi32(fb_pixel_g4i, fb_pixel_a4i);
            __m128i temp_ga_hi = _mm_unpackhi_epi32(fb_pixel_g4i, fb_pixel_a4i);

            __m128i fb_pixel_0 = _mm_unpacklo_epi32(temp_br_lo, temp_ga_lo);
            __m128i fb_pixel_1 = _mm_unpackhi_epi32(temp_br_lo, temp_ga_lo);
            __m128i fb_pixel_2 = _mm_unpacklo_epi32(temp_br_hi, temp_ga_hi);
            __m128i fb_pixel_3 = _mm_unpackhi_epi32(temp_br_hi, temp_ga_hi);

            // Squash the ARGB values down to 32 bits in each SSE "slot"
            // eg. 000b 000g 000r 000a -> bgra bgra bgra bgra
            fb_pixel_0 = _mm_packs_epi32(fb_pixel_0, fb_pixel_0);
            fb_pixel_0 = _mm_packus_epi16(fb_pixel_0, fb_pixel_0);
            fb_pixel_1 = _mm_packs_epi32(fb_pixel_1, fb_pixel_1);
            fb_pixel_1 = _mm_packus_epi16(fb_pixel_1, fb_pixel_1);
            fb_pixel_2 = _mm_packs_epi32(fb_pixel_2, fb_pixel_2);
            fb_pixel_2 = _mm_packus_epi16(fb_pixel_2, fb_pixel_2);
            fb_pixel_3 = _mm_packs_epi32(fb_pixel_3, fb_pixel_3);
            fb_pixel_3 = _mm_packus_epi16(fb_pixel_3, fb_pixel_3);

            // Convert pixels from BGRA BGRA BGRA to BGRA 0000 0000 0000 etc to make them easy to OR together.
            fb_pixel_0 = _mm_and_si128(fb_pixel_0, first_mask);
            fb_pixel_1 = _mm_and_si128(fb_pixel_1, second_mask);
            fb_pixel_2 = _mm_and_si128(fb_pixel_2, third_mask);
            fb_pixel_3 = _mm_and_si128(fb_pixel_3, fourth_mask);

            __m128i out = _mm_or_si128(_mm_or_si128(fb_pixel_0, fb_pixel_1), _mm_or_si128(fb_pixel_2, fb_pixel_3));

            __m128i masked_out = _mm_or_si128(_mm_and_si128(write_mask, out),
                                              _mm_andnot_si128(write_mask, fb_pixel_packed));

            if ((x + 8) > fill_rect.x_max) {
                clip_mask = end_clip_mask;
            }
            else {
                clip_mask = _mm_set1_epi32(0xFFFFFFFF);
            }

#ifdef GG_DEBUG
            pixel_t pixels[4];
            u8 *debug_out = (u8 *)&masked_out;
            u32 mask_index = 0;
            for (u32 i = 0; i < 4; ++i) {
                pixels[i].b = debug_out[mask_index++];
                pixels[i].g = debug_out[mask_index++];
                pixels[i].r = debug_out[mask_index++];
                pixels[i].a = debug_out[mask_index++];
            }
#endif

            _mm_store_si128(fb_pixel, masked_out);
        }
    }
    END_COUNTER_N(process_pixel, aabb2i_clamped_area(fill_rect));
    END_COUNTER(render_image);
}

static void render_image_naive(render_cmd_image_t *cmd,
                               camera_t *cam,
                               game_frame_buffer_t *frame_buffer,
                               aabb2i_t clip_rect)
{
    START_COUNTER(render_image);
    basis_t basis = cmd->header.basis;

    v2 origin = v2_mul(basis.origin, cam->units_to_pixels);
    v2 x_axis = v2_mul(basis.x_axis, cam->units_to_pixels);
    v2 y_axis = v2_mul(basis.y_axis, cam->units_to_pixels);

    aabb2i_t fill_rect = aabb2i_inverted_infinity();

    // NOTE(Wes): Find the max rect we could possible draw in regardless
    // of rotation.
    v2i floor_bounds[4];
    floor_bounds[0] = v2_floor(origin);
    floor_bounds[1] = v2_floor(v2_add(origin, x_axis));
    floor_bounds[2] = v2_floor(v2_add(origin, y_axis));
    floor_bounds[3] = v2_floor(v2_add(v2_add(origin, y_axis), x_axis));

    v2i ceil_bounds[4];
    ceil_bounds[0] = v2_ceil(origin);
    ceil_bounds[1] = v2_ceil(v2_add(origin, x_axis));
    ceil_bounds[2] = v2_ceil(v2_add(origin, y_axis));
    ceil_bounds[3] = v2_ceil(v2_add(v2_add(origin, y_axis), x_axis));

    for (u32 i = 0; i < 4; ++i) {
        v2i floor_bound = floor_bounds[i];
        v2i ceil_bound = ceil_bounds[i];
        if (fill_rect.x_min > floor_bound.x) {
            fill_rect.x_min = floor_bound.x;
        }
        if (fill_rect.y_min > floor_bound.y) {
            fill_rect.y_min = floor_bound.y;
        }
        if (fill_rect.x_max < ceil_bound.x) {
            fill_rect.x_max = ceil_bound.x;
        }
        if (fill_rect.y_max < ceil_bound.y) {
            fill_rect.y_max = ceil_bound.y;
        }
    }

    fill_rect = aabb2i_intersect(fill_rect, clip_rect);

    f32 inv_x_len_sq = 1.0f / v2_len_sq(x_axis);
    f32 inv_y_len_sq = 1.0f / v2_len_sq(y_axis);
    v2 n_x_axis = v2_mul(x_axis, inv_x_len_sq);
    v2 n_y_axis = v2_mul(y_axis, inv_y_len_sq);

    image_t *texture = cmd->image;
    image_t *normals = cmd->normals;
    v4 tint = cmd->tint;

    u8 *fb_data = frame_buffer->data;
    START_COUNTER(process_pixel);
    for (i32 y = fill_rect.y_min; y < fill_rect.y_max; y++) {
        for (i32 x = fill_rect.x_min; x < fill_rect.x_max; x++) {
            // NOTE(Wes): Take the dot product of the point and the axis
            // perpendicular to the one we are testing to see which side
            // the point lies on. This code moves the point p in an
            // anti-clockwise direction.
            v2 p_orig = v2_sub(V2(x, y), origin);
            f32 u = v2_dot(p_orig, n_x_axis);
            f32 v = v2_dot(p_orig, n_y_axis);
            if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f) {
                // TODO(Wes): Actually clamp the texels for subpixel rendering
                // rather than just shortening the texture.
                f32 texel_x = u * (texture->w - 2.0f);
                f32 texel_y = v * (texture->h - 2.0f);

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

                v4 blended_color = v4_lerp(
                    v4_lerp(texel_a, texel_b, fraction_x), v4_lerp(texel_c, texel_d, fraction_x), fraction_y);
                if (blended_color.a == 0.0f) {
                    continue;
                }

                v3 light_intensity = V3(1.0f, 1.0f, 1.0f);

#if 0
                // NOTE(Wes): If no normals are supplied then we use a default
                //            normal that points straigh out the screen.
                // TODO(Wes): Create light volumes out of convex shapes.
                //            We can then use SAT collision detection to determine
                //            which entities the light is affecting.
                v3 normal = V3(0.5f, 0.5f, 1.0f);
                if (normals) {
                    u32 *normal255 = &normals->data[texture_y * texture->w + texture_x];
                    normal = read_image_color(*normal255).rgb;
                }

                // Convert the normal from the range 0,1 to -1,1
                normal = v3_sub(v3_mul(normal, 2.0f), V3(1.0f, 1.0f, 1.0f));
                normal = v3_normalize(normal);
                u32 num_lights = cmd->num_lights;
                v3 light_intensity = V3(0.0f, 0.0f, 0.0f);
                for (u32 i = 0; i < num_lights; ++i) {
                    light_t *light = &cmd->lights[i];

                    v3 light_pos = v3_mul(light->position, cam->units_to_pixels);
                    v3 light_dir = v3_sub(light_pos, V3(x, y, 0.0f)); // TODO(Wes): Objects in the world need a Z depth.
                    f32 light_distance = v3_len(light_dir);
                    light_dir = v3_normalize(light_dir);

                    f32 light_factor = v3_dot(normal, light_dir);
                    v3 ambient_color = v3_mul(light->ambient.rgb, light->ambient.a);
                    v3 light_color = v3_mul(light->color.rgb, light->color.a);
                    v3 diffuse = v3_mul(light_color, kmax(light_factor, 0.0f));

                    f32 attenuation =
                        kclampf(1.0f - (ksq(light_distance) / ksq(light->radius * cam->units_to_pixels)), 0.0f, 1.0f);
                    attenuation *= attenuation;

                    v3 intensity = v3_mul(v3_clamp(v3_add(diffuse, ambient_color), 0.0f, 1.0f), attenuation);
                    light_intensity = v3_add(light_intensity, intensity);
                }
#endif

                u32 *fb_pixel = (u32 *)&fb_data[(x * GG_BYTES_PP) + y * frame_buffer->pitch];
                v4 dest_color = read_frame_buffer_color(*fb_pixel);

                dest_color = srgb_to_linear(dest_color);
                dest_color = linear_blend_tint(blended_color, tint, dest_color);
                dest_color = linear_to_srgb(dest_color);
                dest_color.rgb = v3_hadamard(dest_color.rgb, light_intensity);

                *fb_pixel = color_frame_buffer_u32(dest_color);
            }
        }
    }

    END_COUNTER_N(process_pixel, aabb2i_clamped_area(fill_rect));
    END_COUNTER(render_image);
}

static void render_rect(render_cmd_rect_t *cmd, camera_t *cam, game_frame_buffer_t *frame_buffer, aabb2i_t clip_rect)
{
    basis_t basis = cmd->header.basis;

    v2 origin = v2_mul(basis.origin, cam->units_to_pixels);
    v2 x_axis = v2_mul(basis.x_axis, cam->units_to_pixels);
    v2 y_axis = v2_mul(basis.y_axis, cam->units_to_pixels);
    f32 border_size = cmd->border_size * cam->units_to_pixels;

    v2 hollow_origin = V2(origin.x + border_size, origin.y + border_size);
    f32 hollow_x_len = v2_len(x_axis) - border_size * 2;
    v2 hollow_x_axis = v2_mul(v2_normalize(x_axis), hollow_x_len);
    f32 hollow_y_len = v2_len(y_axis) - border_size * 2;
    v2 hollow_y_axis = v2_mul(v2_normalize(y_axis), hollow_y_len);


    aabb2i_t fill_rect = aabb2i_inverted_infinity();

    // NOTE(Wes): Find the max rect we could possible draw in regardless
    v2i floor_bounds[4];
    floor_bounds[0] = v2_floor(origin);
    floor_bounds[1] = v2_floor(v2_add(origin, x_axis));
    floor_bounds[2] = v2_floor(v2_add(origin, y_axis));
    floor_bounds[3] = v2_floor(v2_add(v2_add(origin, y_axis), x_axis));

    v2i ceil_bounds[4];
    ceil_bounds[0] = v2_ceil(origin);
    ceil_bounds[1] = v2_ceil(v2_add(origin, x_axis));
    ceil_bounds[2] = v2_ceil(v2_add(origin, y_axis));
    ceil_bounds[3] = v2_ceil(v2_add(v2_add(origin, y_axis), x_axis));

    for (u32 i = 0; i < 4; ++i) {
        v2i floor_bound = floor_bounds[i];
        v2i ceil_bound = ceil_bounds[i];
        if (fill_rect.x_min > floor_bound.x) {
            fill_rect.x_min = floor_bound.x;
        }
        if (fill_rect.y_min > floor_bound.y) {
            fill_rect.y_min = floor_bound.y;
        }
        if (fill_rect.x_max < ceil_bound.x) {
            fill_rect.x_max = ceil_bound.x;
        }
        if (fill_rect.y_max < ceil_bound.y) {
            fill_rect.y_max = ceil_bound.y;
        }
    }

    fill_rect = aabb2i_intersect(fill_rect, clip_rect);
    if (!aabb2i_has_area(fill_rect)) {
        return;
    }

    f32 inv_x_len_sq = 1.0f / v2_len_sq(x_axis);
    f32 inv_y_len_sq = 1.0f / v2_len_sq(y_axis);
    v2 n_x_axis = v2_mul(x_axis, inv_x_len_sq);
    v2 n_y_axis = v2_mul(y_axis, inv_y_len_sq);

    f32 hollow_inv_x_len_sq = 1.0f / v2_len_sq(hollow_x_axis);
    f32 hollow_inv_y_len_sq = 1.0f / v2_len_sq(hollow_y_axis);
    v2 hollow_n_x_axis = v2_mul(hollow_x_axis, hollow_inv_x_len_sq);
    v2 hollow_n_y_axis = v2_mul(hollow_y_axis, hollow_inv_y_len_sq);

    v4 color = cmd->color;
    if (border_size > 0.0f)
    {
        u8 *fb_data = frame_buffer->data;
        for (i32 y = fill_rect.y_min; y < fill_rect.y_max; y++) {
            for (i32 x = fill_rect.x_min; x < fill_rect.x_max; x++) {
                v2 p_orig = v2_sub(V2(x, y), origin);
                f32 u = v2_dot(p_orig, n_x_axis);
                f32 v = v2_dot(p_orig, n_y_axis);
                if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f) {
                    v2 hollow_orig = v2_sub(V2(x, y), hollow_origin);
                    f32 hollow_u = v2_dot(hollow_orig, hollow_n_x_axis);
                    f32 hollow_v = v2_dot(hollow_orig, hollow_n_y_axis);
                    if (!(hollow_u >= 0.0f && hollow_u <= 1.0f && hollow_v >= 0.0f && hollow_v <= 1.0f)) {
                        u32 *fb_pixel = (u32 *)&fb_data[(x * GG_BYTES_PP) + y * frame_buffer->pitch];
                        v4 dest_color = read_frame_buffer_color(*fb_pixel);
                        dest_color = srgb_to_linear(dest_color);
                        dest_color = linear_blend(color, dest_color);
                        dest_color = linear_to_srgb(dest_color);
                        *fb_pixel = color_frame_buffer_u32(dest_color);
                    }
                }
            }
        }
    }
    else
    {
        u8 *fb_data = frame_buffer->data;
        for (i32 y = fill_rect.y_min; y < fill_rect.y_max; y++) {
            for (i32 x = fill_rect.x_min; x < fill_rect.x_max; x++) {
                v2 p_orig = v2_sub(V2(x, y), origin);
                f32 u = v2_dot(p_orig, n_x_axis);
                f32 v = v2_dot(p_orig, n_y_axis);
                if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f) {
                    u32 *fb_pixel = (u32 *)&fb_data[(x * GG_BYTES_PP) + y * frame_buffer->pitch];
                    v4 dest_color = read_frame_buffer_color(*fb_pixel);
                    dest_color = srgb_to_linear(dest_color);
                    dest_color = linear_blend(color, dest_color);
                    dest_color = linear_to_srgb(dest_color);
                    *fb_pixel = color_frame_buffer_u32(dest_color);
                }
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
    render_cmd_clear_t *cmd = (render_cmd_clear_t *)render_push_cmd(queue, sizeof(render_cmd_clear_t));
    cmd->header.type = render_type_clear;
    cmd->color = color;
}

void render_push_image(render_queue_t *queue,
                       basis_t *basis,
                       v4 tint,
                       image_t *image,
                       image_t *normals,
                       light_t *lights,
                       int num_lights)
{
    render_cmd_image_t *cmd = (render_cmd_image_t *)render_push_cmd(queue, sizeof(render_cmd_image_t));
    cmd->header.type = render_type_image;
    cmd->header.basis = *basis;
    cmd->tint = tint;
    cmd->image = image;
    cmd->normals = normals;
    cmd->lights = lights;
    cmd->num_lights = num_lights;
}

void render_push_rect(render_queue_t *queue, basis_t *basis, v4 color)
{
    render_cmd_rect_t *cmd = (render_cmd_rect_t *)render_push_cmd(queue, sizeof(render_cmd_rect_t));
    cmd->header.type = render_type_rect;
    cmd->header.basis = *basis;
    cmd->color = color;
    cmd->border_size = 0.0f;
}

void render_push_hollow_rect(render_queue_t *queue, basis_t *basis, v4 color, f32 border_size)
{
    render_cmd_rect_t *cmd = (render_cmd_rect_t *)render_push_cmd(queue, sizeof(render_cmd_rect_t));
    cmd->header.type = render_type_rect;
    cmd->header.basis = *basis;
    cmd->color = color;
    cmd->border_size = border_size;
}

render_queue_t *render_alloc_queue(memory_arena_t *arena, u32 max_render_queue_size, camera_t *cam)
{
    render_queue_t *queue = push_struct(arena, render_queue_t);
    queue->size = max_render_queue_size;
    queue->index = 0;
    queue->base = push_size(arena, max_render_queue_size);
    queue->camera = cam;
    return queue;
}

typedef struct {
    render_queue_t *queue;
    game_frame_buffer_t *frame_buffer;
    aabb2i_t clip_rect;
} render_work_t;

void render_draw_tile(render_queue_t *queue,
                      game_frame_buffer_t *frame_buffer,
                      aabb2i_t clip_rect)
{
    START_COUNTER(render_draw_queue);
    for (u32 address = 0; address < queue->index;) {
        render_cmd_header_t *header = (render_cmd_header_t *)(queue->base + address);
        switch (header->type) {
        case render_type_clear:
            render_clear((render_cmd_clear_t *)header, frame_buffer, clip_rect);
            address += sizeof(render_cmd_clear_t);
            break;
        case render_type_image:
            render_image((render_cmd_image_t *)header, queue->camera, frame_buffer, clip_rect);
            // render_image_naive((render_cmd_image_t *)header, queue->camera, frame_buffer, clip_rect);
            address += sizeof(render_cmd_image_t);
            break;
        case render_type_rect:
            render_rect((render_cmd_rect_t *)header, queue->camera, frame_buffer, clip_rect);
            address += sizeof(render_cmd_rect_t);
            break;
        }
    }

    END_COUNTER(render_draw_queue);
}

void render_worker(void *data)
{
    render_work_t *work = (render_work_t *)data;
    render_draw_tile(work->queue, work->frame_buffer, work->clip_rect);
}

void render_draw_queue(render_queue_t *queue, game_frame_buffer_t *frame_buffer, game_work_queues_t *work_queues)
{
    assert(((uintptr_t)frame_buffer->data & 15) == 0);
    #define tile_y_count 4
    #define tile_x_count 4

    u32 fb_width = frame_buffer->w;
    u32 fb_height = frame_buffer->h;

    u32 tile_height = fb_height / tile_y_count;
    u32 tile_width = fb_width / tile_x_count;

    // Round tile_width to the nearest multiple of 4 because we always render 4 pixel at a time.
    tile_width = ((tile_width + 3) / 4) * 4;

    render_work_t work_infos[tile_y_count * tile_x_count];
    u32 work_index = 0;
    for (u32 y = 0; y < tile_y_count; ++y) {
        for (u32 x = 0; x < tile_x_count; ++x) {
            aabb2i_t clip_rect;
            clip_rect.y_min = y * tile_height;
            clip_rect.x_min = x * tile_width;
            clip_rect.y_max = clip_rect.y_min + tile_height;
            clip_rect.x_max = clip_rect.x_min + tile_width;

            if (clip_rect.x_max > (i32)fb_width) {
                clip_rect.x_max = fb_width;
            }

            render_work_t *data = work_infos + work_index++;
            data->queue = queue;
            data->frame_buffer = frame_buffer;
            data->clip_rect = clip_rect;

            //render_worker(queue, frame_buffer, clip_rect);
            work_queues->add_work(work_queues->render_work_queue, render_worker, data);
        }
    }

    work_queues->finish_work(work_queues->render_work_queue);
    queue->index = 0;
}
