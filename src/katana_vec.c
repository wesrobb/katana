#include "katana_types.h"
#include "katana_intrinsics.h"

v2 v2_zero()
{
        v2 dst = {0, 0};
        return dst;
}

v2 v2_add(v2 a, v2 b)
{
        v2 sum;
        sum.x = a.x + b.x;
        sum.y = a.y + b.y;
        return sum;
}

v2 v2_sub(v2 a, v2 b)
{
        v2 diff;
        diff.x = a.x - b.x;
        diff.y = a.y - b.y;
        return diff;
}

v2 v2_mul(v2 a, f32 scalar)
{
        v2 product;
        product.x = a.x * scalar;
        product.y = a.y * scalar;
        return product;
}

v2 v2_div(v2 a, f32 scalar)
{
        v2 quotient;
        quotient.x = a.x / scalar;
        quotient.y = a.y / scalar;
        return quotient;
}

f32 v2_dot(v2 a, v2 b)
{
        return a.x * b.x + a.y + b.y;
}

void v2_round2(v2 a, v2 b, v2i *a_result, v2i *b_result)
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

void v2_floor2(v2 a, v2 b, v2i *a_result, v2i *b_result)
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

void v2_ceil2(v2 a, v2 b, v2i *a_result, v2i *b_result)
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
v2i v2_sign(v2 src)
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

v2 v2_lerp(v2 v0, v2 v1, f32 t)
{
        return v2_add(v2_mul(v1, t), v2_mul(v0, 1.0f - t));
}
