#pragma once

#include "gg_platform.h"
#include "gg_math.h"

// Note(Wes): Vectors
typedef struct {
    i32 x;
    i32 y;
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
            x, y                                                                                                       \
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

static inline v2 v2_add(v2 a, v2 b)
{
    return V2(a.x + b.x, a.y + b.y);
}

static inline v2 v2_sub(v2 a, v2 b)
{
    return V2(a.x - b.x, a.y - b.y);
}

static inline v2 v2_mul(v2 a, f32 scalar)
{
    return V2(a.x * scalar, a.y * scalar);
}

static inline v2 v2_div(v2 a, f32 scalar)
{
    return V2(a.x / scalar, a.y / scalar);
}

static inline v2 v2_perp(v2 a)
{
    return V2(-a.y, a.x);
}

static inline v2 v2_neg(v2 a)
{
    return V2(-a.x, -a.y);
}

static inline f32 v2_dot(v2 a, v2 b)
{
    return (a.x * b.x) + (a.y * b.y);
}

static inline f32 v2_len_sq(v2 a)
{
    return v2_dot(a, a);
}

static inline f32 v2_len(v2 a)
{
    return ksqrt(v2_len_sq(a));
}

static inline v2 v2_normalize(v2 a)
{
    return v2_div(a, v2_len(a));
}

static inline void v2_round2(v2 a, v2 b, v2i *a_result, v2i *b_result)
{
    assert(a_result);
    assert(b_result);

    f32 lhs[4];
    f32 results[4];

    lhs[0] = a.x;
    lhs[1] = a.y;
    lhs[2] = b.x;
    lhs[3] = b.y;

    round4f(lhs, results);

    a_result->x = (i32)results[0];
    a_result->y = (i32)results[1];
    b_result->x = (i32)results[2];
    b_result->y = (i32)results[3];
}

static inline void v2_floor2(v2 a, v2 b, v2i *a_result, v2i *b_result)
{
    assert(a_result);
    assert(b_result);

    f32 lhs[4];
    f32 results[4];

    lhs[0] = a.x;
    lhs[1] = a.y;
    lhs[2] = b.x;
    lhs[3] = b.y;

    floor4f(lhs, results);

    a_result->x = (i32)results[0];
    a_result->y = (i32)results[1];
    b_result->x = (i32)results[2];
    b_result->y = (i32)results[3];
}

static inline void v2_ceil2(v2 a, v2 b, v2i *a_result, v2i *b_result)
{
    assert(a_result);
    assert(b_result);

    f32 lhs[4];
    f32 results[4];

    lhs[0] = a.x;
    lhs[1] = a.y;
    lhs[2] = b.x;
    lhs[3] = b.y;

    ceil4f(lhs, results);

    a_result->x = (i32)results[0];
    a_result->y = (i32)results[1];
    b_result->x = (i32)results[2];
    b_result->y = (i32)results[3];
}

// Converts the specified vector into 1 on each axis preserving the sign.
static inline v2i v2_sign(v2 src)
{
    v2i result = {};
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

static inline v2 v2_lerp(v2 v0, v2 v1, f32 t)
{
    return v2_add(v2_mul(v1, t), v2_mul(v0, 1.0f - t));
}

static inline v3 v3_add(v3 a, v3 b)
{
    return V3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline v3 v3_sub(v3 a, v3 b)
{
    return V3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline v3 v3_mul(v3 a, f32 scalar)
{
    return V3(a.x * scalar, a.y * scalar, a.z * scalar);
}

static inline v3 v3_div(v3 a, f32 scalar)
{
    return V3(a.x / scalar, a.y / scalar, a.z / scalar);
}

static inline f32 v3_dot(v3 a, v3 b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

static inline f32 v3_len_sq(v3 a)
{
    return v3_dot(a, a);
}

static inline f32 v3_len(v3 a)
{
    return ksqrt(v3_len_sq(a));
}

static inline v3 v3_normalize(v3 a)
{
    return v3_div(a, v3_len(a));
}

static inline v3 v3_hadamard(v3 a, v3 b)
{
    return V3(a.x * b.x, a.y * b.y, a.z * b.z);
}

static inline v3 v3_clamp(v3 a, f32 min, f32 max)
{
    return V3(kclampf(a.x, min, max), kclampf(a.y, min, max), kclampf(a.z, min, max));
}

static inline v4 v4_add(v4 a, v4 b)
{
    return V4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

static inline v4 v4_sub(v4 a, v4 b)
{
    return V4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

static inline v4 v4_mul(v4 a, f32 scalar)
{
    return V4(a.x * scalar, a.y * scalar, a.z * scalar, a.w * scalar);
}

static inline v4 v4_div(v4 a, f32 scalar)
{
    return V4(a.x / scalar, a.y / scalar, a.z / scalar, a.w / scalar);
}

static inline v4 v4_lerp(v4 v0, v4 v1, f32 t)
{
    return v4_add(v4_mul(v1, t), v4_mul(v0, 1.0f - t));
}

static inline f32 v4_dot(v4 a, v4 b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w);
}

static inline v4 v4_hadamard(v4 a, v4 b)
{
    return V4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

static inline f32 v4_len_sq(v4 a)
{
    return v4_dot(a, a);
}

static inline f32 v4_len(v4 a)
{
    return ksqrt(v4_len_sq(a));
}

static inline v4 v4_normalize(v4 a)
{
    return v4_div(a, v4_len(a));
}
