#pragma once

#include "katana_platform.h"

#include <immintrin.h>

static inline void copy4f(f32 const *in, f32 *out)
{
        // NOTE(Wes): store_ps and load_ps require 16 byte alignment.
        assert(is_aligned(in, 16));
        assert(is_aligned(out, 16));
        __m128 packed_args = _mm_load_ps(in);
        _mm_store_ps(out, packed_args);
}

static inline void round4f(f32 const *in, f32 *out)
{
        assert(is_aligned(in, 16));
        assert(is_aligned(out, 16));
        __m128 packed_args = _mm_load_ps(in);
        __m128 packed_result = _mm_round_ps(packed_args, _MM_FROUND_TO_NEAREST_INT);
        _mm_store_ps(out, packed_result);
}

static inline void add4f(f32 const *a, f32 const *b, f32 *result)
{
        assert(is_aligned(a, 16));
        assert(is_aligned(b, 16));
        assert(is_aligned(result, 16));
        __m128 a_packed_args = _mm_load_ps(a);
        __m128 b_packed_args = _mm_load_ps(b);
        __m128 packed_result = _mm_add_ps(a_packed_args, b_packed_args);
        _mm_store_ps(result, packed_result);
}

static inline void sub4f(f32 const *a, f32 const *b, f32 *result)
{
        assert(is_aligned(a, 16));
        assert(is_aligned(b, 16));
        assert(is_aligned(result, 16));
        __m128 a_packed_args = _mm_load_ps(a);
        __m128 b_packed_args = _mm_load_ps(b);
        __m128 packed_result = _mm_sub_ps(a_packed_args, b_packed_args);
        _mm_store_ps(result, packed_result);
}

static inline void mul4f(f32 const *a, f32 const *b, f32 *result)
{
        assert(is_aligned(a, 16));
        assert(is_aligned(b, 16));
        assert(is_aligned(result, 16));
        __m128 a_packed_args = _mm_load_ps(a);
        __m128 b_packed_args = _mm_load_ps(b);
        __m128 packed_result = _mm_mul_ps(a_packed_args, b_packed_args);
        _mm_store_ps(result, packed_result);
}

static inline void div4f(f32 const *a, f32 const *b, f32 *result)
{
        assert(is_aligned(a, 16));
        assert(is_aligned(b, 16));
        assert(is_aligned(result, 16));
        __m128 a_packed_args = _mm_load_ps(a);
        __m128 b_packed_args = _mm_load_ps(b);
        __m128 packed_result = _mm_div_ps(a_packed_args, b_packed_args);
        _mm_store_ps(result, packed_result);
}
