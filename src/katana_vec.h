#pragma once

#include "katana_types.h"
#include "katana_intrinsics.h"

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
