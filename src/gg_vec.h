#pragma once

#include "gg_platform.h"
#include "gg_math.h"

#include <immintrin.h>

// Note(Wes): Vectors
typedef union {
	struct {
		i32 x, y;
	};
    i32 v[2];
} v2i;

typedef union {
    struct {
        f32 x, y;
    };
    f32 v[2];
} v2;
#define V2(x, y)                                                                                                       \
    (v2)                                                                                                               \
    {                                                                                                                  \
        {                                                                                                              \
            (f32)(x), (f32)(y)                                                                                         \
        }                                                                                                              \
    }

#define V2I(x, y)                                                                                                      \
    (v2i)                                                                                                              \
    {                                                                                                                  \
        {                                                                                                              \
            (i32)(x), (i32)(y)                                                                                         \
        }                                                                                                              \
    }

typedef union {
    struct {
        f32 x, y, z;
    };
    struct {
        f32 r, g, b;
    };
    struct {
        v2 xy;
    };
    struct {
        f32 x_;
        v2 yz;
    };
    f32 v[3];
} v3;
#define V3(x, y, z)                                                                                                    \
    (v3)                                                                                                               \
    {                                                                                                                  \
        {                                                                                                              \
            x, y, z                                                                                                    \
        }                                                                                                              \
    }

typedef union {
    struct {
        f32 x, y, z, w;
    };
    struct {
        f32 r, g, b, a;
    };
    struct {
        v2 xy, zw;
    };
    struct {
        v3 xyz;
        f32 w_;
    };
    struct {
        v3 rgb;
        f32 a_;
    };
    struct {
        f32 x_;
        v3 yzw;
    };
    struct {
        f32 x__;
        v2 yz;
        f32 w__;
    };
    f32 v[4];
} v4;
#define V4(x, y, z, w)                                                                                                 \
    (v4)                                                                                                               \
    {                                                                                                                  \
        {                                                                                                              \
            x, y, z, w                                                                                                 \
        }                                                                                                              \
    }
#define COLOR(r, g, b, a)                                                                                              \
    (v4)                                                                                                               \
    {                                                                                                                  \
        {                                                                                                              \
            r, g, b, a                                                                                                 \
        }                                                                                                              \
    }

inline v2 v2_add(v2 a, v2 b)
{
    return V2(a.x + b.x, a.y + b.y);
}

inline v2 v2_sub(v2 a, v2 b)
{
    return V2(a.x - b.x, a.y - b.y);
}

inline v2 v2_mul(v2 a, f32 scalar)
{
    return V2(a.x * scalar, a.y * scalar);
}

inline v2 v2_div(v2 a, f32 scalar)
{
    return V2(a.x / scalar, a.y / scalar);
}

inline v2 v2_perp(v2 a)
{
    return V2(-a.y, a.x);
}

inline v2 v2_neg(v2 a)
{
    return V2(-a.x, -a.y);
}

inline f32 v2_dot(v2 a, v2 b)
{
    return (a.x * b.x) + (a.y * b.y);
}

inline f32 v2_len_sq(v2 a)
{
    return v2_dot(a, a);
}

inline f32 v2_len(v2 a)
{
    return ksqrt(v2_len_sq(a));
}

inline v2 v2_normalize(v2 a)
{
    return v2_div(a, v2_len(a));
}

inline v2i v2_floor(v2 a)
{
	return V2I(kfloor(a.x), kfloor(a.y));
}

inline v2i v2_ceil(v2 a)
{
	return V2I(kceil(a.x), kceil(a.y));
}

inline void v2_round2(v2 a, v2 b, v2i *a_result, v2i *b_result)
{
    assert(a_result);
    assert(b_result);

    f32 lhs[4];
    f32 results[4];

    lhs[0] = a.x;
    lhs[1] = a.y;
    lhs[2] = b.x;
    lhs[3] = b.y;

	__m128 packed_args = _mm_load_ps(lhs);
	__m128 packed_result = _mm_round_ps(packed_args, _MM_FROUND_TO_NEAREST_INT);
	_mm_store_ps(results, packed_result);

    a_result->x = (i32)results[0];
    a_result->y = (i32)results[1];
    b_result->x = (i32)results[2];
    b_result->y = (i32)results[3];
}

inline void v2_floor2(v2 a, v2 b, v2i *a_result, v2i *b_result)
{
    assert(a_result);
    assert(b_result);

    f32 lhs[4];
    f32 results[4];

    lhs[0] = a.x;
    lhs[1] = a.y;
    lhs[2] = b.x;
    lhs[3] = b.y;

	__m128 packed_args = _mm_load_ps(lhs);
	__m128 packed_result = _mm_floor_ps(packed_args);
	_mm_store_ps(results, packed_result);

    a_result->x = (i32)results[0];
    a_result->y = (i32)results[1];
    b_result->x = (i32)results[2];
    b_result->y = (i32)results[3];
}

inline void v2_ceil2(v2 a, v2 b, v2i *a_result, v2i *b_result)
{
    assert(a_result);
    assert(b_result);

    f32 lhs[4];
    f32 results[4];

    lhs[0] = a.x;
    lhs[1] = a.y;
    lhs[2] = b.x;
    lhs[3] = b.y;

    __m128 packed_args = _mm_load_ps(lhs);
	__m128 packed_result = _mm_ceil_ps(packed_args);
	_mm_store_ps(results, packed_result);;

    a_result->x = (i32)results[0];
    a_result->y = (i32)results[1];
    b_result->x = (i32)results[2];
    b_result->y = (i32)results[3];
}

// Converts the specified vector into 1 on each axis preserving the sign.
inline v2i v2_sign(v2 src)
{
    v2i result = {0};
    if (src.x > 0.0f) {
        result.x = 1;
    } else if (src.x < 0.0f) {
        result.x = -1;
    }
    if (src.y > 0.0f) {
        result.y = 1;
    } else if (src.y < 0.0f) {
        result.y = -1;
    }
    return result;
}

inline v2 v2_lerp(v2 v0, v2 v1, f32 t)
{
    return v2_add(v2_mul(v1, t), v2_mul(v0, 1.0f - t));
}

inline v3 v3_add(v3 a, v3 b)
{
    return V3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline v3 v3_sub(v3 a, v3 b)
{
    return V3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline v3 v3_mul(v3 a, f32 scalar)
{
    return V3(a.x * scalar, a.y * scalar, a.z * scalar);
}

inline v3 v3_div(v3 a, f32 scalar)
{
    return V3(a.x / scalar, a.y / scalar, a.z / scalar);
}

inline f32 v3_dot(v3 a, v3 b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

inline f32 v3_len_sq(v3 a)
{
    return v3_dot(a, a);
}

inline f32 v3_len(v3 a)
{
    return ksqrt(v3_len_sq(a));
}

inline v3 v3_normalize(v3 a)
{
    return v3_div(a, v3_len(a));
}

inline v3 v3_hadamard(v3 a, v3 b)
{
    return V3(a.x * b.x, a.y * b.y, a.z * b.z);
}

inline v3 v3_clamp(v3 a, f32 min, f32 max)
{
    return V3(kclampf(a.x, min, max), kclampf(a.y, min, max), kclampf(a.z, min, max));
}

inline v4 v4_add(v4 a, v4 b)
{
    return V4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

inline v4 v4_sub(v4 a, v4 b)
{
    return V4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

inline v4 v4_mul(v4 a, f32 scalar)
{
    return V4(a.x * scalar, a.y * scalar, a.z * scalar, a.w * scalar);
}

inline v4 v4_div(v4 a, f32 scalar)
{
    return V4(a.x / scalar, a.y / scalar, a.z / scalar, a.w / scalar);
}

inline v4 v4_lerp(v4 v0, v4 v1, f32 t)
{
    return v4_add(v4_mul(v1, t), v4_mul(v0, 1.0f - t));
}

inline f32 v4_dot(v4 a, v4 b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w);
}

inline v4 v4_hadamard(v4 a, v4 b)
{
    return V4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

inline f32 v4_len_sq(v4 a)
{
    return v4_dot(a, a);
}

inline f32 v4_len(v4 a)
{
    return ksqrt(v4_len_sq(a));
}

inline v4 v4_normalize(v4 a)
{
    return v4_div(a, v4_len(a));
}
