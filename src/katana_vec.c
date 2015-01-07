#include "katana_types.h"
#include "katana_intrinsics.h"

#include <immintrin.h>

vec2f_t vec2f_copy(vec2f_t src)
{
        vec2f_t dst = {src.x, src.y};
        return dst;
}

vec2f_t vec2f_add(vec2f_t a, vec2f_t b)
{
        vec2f_t sum;
        sum.x = a.x + b.x;
        sum.y = a.y + b.y;
        return sum;
}

vec2f_t vec2f_sub(vec2f_t a, vec2f_t b)
{
        vec2f_t diff;
        diff.x = a.x - b.x;
        diff.y = a.y - b.y;
        return diff;
}

vec2f_t vec2f_mul(vec2f_t a, f32 scalar)
{
        vec2f_t product;
        product.x = a.x * scalar;
        product.y = a.y * scalar;
        return product;
}

vec2f_t vec2f_div(vec2f_t a, f32 scalar)
{
        vec2f_t quotient;
        quotient.x = a.x / scalar;
        quotient.y = a.y / scalar;
        return quotient;
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
