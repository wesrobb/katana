#pragma once

#include <immintrin.h>
#include <math.h>

#include "gg_types.h"

static inline f32 kabsf(f32 f)
{
    i32 i = ((*(int *)&f) & 0x7fffffff);
    return (*(float *)&i);
}

static inline f32 kcosf(f32 theta)
{
    return cosf(theta);
}

static inline f32 ksinf(f32 theta)
{
    return sinf(theta);
}

static inline f32 kclampf(f32 value, f32 min, f32 max)
{
    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }

    return value;
}

static inline u32 kclamp(u32 value, u32 min, u32 max)
{
    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }

    return value;
}

static inline f32 klerp(f32 a, f32 b, f32 t)
{
    return (1.0f - t) * a + t * b;
}

static inline f32 ksqrt(f32 a)
{
    return sqrtf(a);
}

static inline f32 kmin(f32 a, f32 b)
{
    return a < b ? a : b;
}

static inline f32 kmax(f32 a, f32 b)
{
    return a > b ? a : b;
}

static inline void round4f(f32 const *in, f32 *out)
{
    assert(is_aligned(in, 16));
    assert(is_aligned(out, 16));
    __m128 packed_args = _mm_load_ps(in);
    __m128 packed_result = _mm_round_ps(packed_args, _MM_FROUND_TO_NEAREST_INT);
    _mm_store_ps(out, packed_result);
}

static inline void floor4f(f32 const *in, f32 *out)
{
    assert(is_aligned(in, 16));
    assert(is_aligned(out, 16));
    __m128 packed_args = _mm_load_ps(in);
    __m128 packed_result = _mm_floor_ps(packed_args);
    _mm_store_ps(out, packed_result);
}

static inline void ceil4f(f32 const *in, f32 *out)
{
    assert(is_aligned(in, 16));
    assert(is_aligned(out, 16));
    __m128 packed_args = _mm_load_ps(in);
    __m128 packed_result = _mm_ceil_ps(packed_args);
    _mm_store_ps(out, packed_result);
}
