#pragma once

#include "katana_platform.h"

#include <immintrin.h>

static inline void k_roundf4(f32 *restrict in, f32 *restrict out)
{
        // NOTE(Wes): store_ps and load_ps require 16 byte alignment.
        assert(is_aligned(in, 16));
        assert(is_aligned(out, 16));
        __m128 packed_args = _mm_load_ps(in);
        __m128 packed_result = _mm_round_ps(packed_args, _MM_FROUND_TO_NEAREST_INT);
        _mm_store_ps(out, packed_result);
}
