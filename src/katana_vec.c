#include "katana_types.h"
#include "katana_intrinsics.h"

void vec2f_copy(vec2f_t *dst, vec2f_t src)
{
        f32 lhs[4];
        f32 rhs[4];

        lhs[0] = src.x;
        lhs[1] = src.y;
        lhs[2] = 0.0f;
        lhs[3] = 0.0f;

        copy4f(lhs, rhs);

        dst->x = rhs[0];
        dst->y = rhs[1];
}

vec2f_t vec2f_add(vec2f_t a, vec2f_t b)
{
        f32 lhs[4];
        f32 rhs[4];
        f32 results[4];

        lhs[0] = a.x;
        lhs[1] = a.y;
        lhs[2] = 0.0f;
        lhs[3] = 0.0f;
        rhs[0] = b.x;
        rhs[1] = b.y;
        rhs[2] = 0.0f;
        rhs[3] = 0.0f;

        add4f(lhs, rhs, results);

        vec2f_t sum;
        sum.x = results[0];
        sum.y = results[1];

        return sum;
}

// Adds 2 sets of vectors together using a single 128 bit instrinsic (2 vec2f's == 128bits).
// a + b = a_plus_b
// c + d = c_plus_d
void vec2f_add2(vec2f_t a, vec2f_t b, vec2f_t c, vec2f_t d, vec2f_t *ab, vec2f_t *cd)
{
        assert(ab);
        assert(cd);

        f32 lhs[4];
        f32 rhs[4];
        f32 results[4];

        lhs[0] = a.x;
        lhs[1] = a.y;
        lhs[2] = c.x;
        lhs[3] = c.y;
        rhs[0] = b.x;
        rhs[1] = b.y;
        rhs[2] = d.x;
        rhs[3] = d.y;

        add4f(lhs, rhs, results);

        ab->x = results[0];
        ab->y = results[1];
        cd->x = results[2];
        cd->y = results[3];
}

vec2f_t vec2f_sub(vec2f_t a, vec2f_t b)
{
        f32 lhs[4];
        f32 rhs[4];
        f32 results[4];

        lhs[0] = a.x;
        lhs[1] = a.y;
        lhs[2] = 0.0f;
        lhs[3] = 0.0f;
        rhs[0] = b.x;
        rhs[1] = b.y;
        rhs[2] = 0.0f;
        rhs[3] = 0.0f;

        sub4f(lhs, rhs, results);

        vec2f_t diff;
        diff.x = results[0];
        diff.y = results[1];

        return diff;
}

// Subtracts 2 sets of vectors from one another using a single 128 bit instrinsic (2 vec2f's == 128bits).
// a - b = a_sub_b
// c - d = c_sub_d
void vec2f_sub2(vec2f_t a, vec2f_t b, vec2f_t c, vec2f_t d, vec2f_t *ab, vec2f_t *cd)
{
        assert(ab);
        assert(cd);

        f32 lhs[4];
        f32 rhs[4];
        f32 results[4];

        lhs[0] = a.x;
        lhs[1] = a.y;
        lhs[2] = c.x;
        lhs[3] = c.y;
        rhs[0] = b.x;
        rhs[1] = b.y;
        rhs[2] = d.x;
        rhs[3] = d.y;

        sub4f(lhs, rhs, results);

        ab->x = results[0];
        ab->y = results[1];
        cd->x = results[2];
        cd->y = results[3];
}

vec2f_t vec2f_mul(vec2f_t a, f32 scalar)
{
        f32 lhs[4];
        f32 rhs[4];
        f32 results[4];

        lhs[0] = a.x;
        lhs[1] = a.y;
        lhs[2] = 0.0f;
        lhs[3] = 0.0f;
        rhs[0] = scalar;
        rhs[1] = scalar;
        rhs[2] = 0.0f;
        rhs[3] = 0.0f;

        mul4f(lhs, rhs, results);

        vec2f_t result;
        result.x = results[0];
        result.y = results[1];
        return result;
}

void vec2f_mul2(vec2f_t a, vec2f_t b, f32 scalar, vec2f_t *a_result, vec2f_t *b_result)
{
        assert(a_result);
        assert(b_result);

        f32 lhs[4];
        f32 rhs[4];
        f32 results[4];

        lhs[0] = a.x;
        lhs[1] = a.y;
        lhs[2] = b.x;
        lhs[3] = b.y;
        rhs[0] = scalar;
        rhs[1] = scalar;
        rhs[2] = scalar;
        rhs[3] = scalar;

        mul4f(lhs, rhs, results);

        a_result->x = results[0];
        a_result->y = results[1];
        b_result->x = results[2];
        b_result->y = results[3];
}

vec2f_t vec2f_div(vec2f_t a, f32 scalar)
{
        f32 lhs[4];
        f32 rhs[4];
        f32 results[4];

        lhs[0] = a.x;
        lhs[1] = a.y;
        lhs[2] = 0.0f;
        lhs[3] = 0.0f;
        rhs[0] = scalar;
        rhs[1] = scalar;
        rhs[2] = 0.0f;
        rhs[3] = 0.0f;

        div4f(lhs, rhs, results);

        vec2f_t result;
        result.x = results[0];
        result.y = results[1];
        return result;
}

void vec2f_div2(vec2f_t a, vec2f_t b, f32 scalar, vec2f_t *a_result, vec2f_t *b_result)
{
        assert(a_result);
        assert(b_result);

        f32 lhs[4];
        f32 rhs[4];
        f32 results[4];

        lhs[0] = a.x;
        lhs[1] = a.y;
        lhs[2] = b.x;
        lhs[3] = b.y;
        rhs[0] = scalar;
        rhs[1] = scalar;
        rhs[2] = scalar;
        rhs[3] = scalar;

        div4f(lhs, rhs, results);

        a_result->x = results[0];
        a_result->y = results[1];
        b_result->x = results[2];
        b_result->y = results[3];
}

void vec2f_round2(vec2f_t a, vec2f_t b, vec2i_t *a_result, vec2i_t *b_result)
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
