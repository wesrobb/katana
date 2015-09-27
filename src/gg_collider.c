#include "gg_collider.h"

typedef struct {
    f32 min, max;
} projection_t;

// We know there must be 4 points so don't bother passing in
// the number of points.
static projection_t project(v2 *points, v2 axis)
{
    f32 min = v2_dot(axis, points[0]);
    f32 max = min;

    for (u32 j = 1; j < 4; j++) {
        f32 projected = v2_dot(axis, points[j]);
        if (projected < min) {
            min = projected;
        } else if (projected > max) {
            max = projected;
        }
    }
    projection_t projection = {min, max};
    return projection;
}

static inline b8 overlap(projection_t p1, projection_t p2)
{
    return p2.max >= p1.min && p2.max >= p1.min;
}

b8 collider_test(basis_t *a, basis_t *b)
{
    assert(a);
    assert(b);

    v2 a_x = v2_add(a->origin, a->x_axis);
    v2 a_y = v2_add(a->origin, a->y_axis);
    v2 a_points[] = {a->origin, a_x, a_y, v2_add(a_x, a->y_axis)};

    v2 b_x = v2_add(b->origin, b->x_axis);
    v2 b_y = v2_add(b->origin, b->y_axis);
    v2 b_points[] = {b->origin, b_x, b_y, v2_add(b_x, b->y_axis)};

    // NOTE(Wes): Calculate normals. Base we are explicitly only supporting
    // rotated rects we only need to find the normal of one of the sides.
    // All other normals can be inferred from there onward.
    // TODO(Wes): Do we want to deal with the case where the axis are not ortho?
    v2 a_normals[] = {v2_normalize(a->x_axis), v2_perp(a_normals[0])};
    v2 b_normals[] = {v2_normalize(b->x_axis), v2_perp(b_normals[0])};

    projection_t p1;
    projection_t p2;
    for (u32 i = 0; i < 2; ++i) {
        v2 a_edge = a_normals[i];
        p1 = project(a_points, a_edge);
        p2 = project(b_points, a_edge);
        if (!overlap(p1, p2)) {
            // TODO(Wes): Calculate MTV
            return 0;
        }
        v2 b_edge = b_normals[i];
        p1 = project(a_points, b_edge);
        p2 = project(b_points, b_edge);
        if (!overlap(p1, p2)) {
            // TODO(Wes): Calculate MTV
            return 0;
        }
    }

    return 1;
}

b8 collider_contains_point(basis_t *a, v2 p)
{
    // NOTE(Wes): Take the dot product of the point and the axis
    // perpendicular to the one we are testing to see which side
    // the point lies on.
    v2 origin = a->origin;
    v2 x_axis = a->x_axis;
    v2 y_axis = a->y_axis;
    p = v2_sub(p, origin);
    f32 edge1 = v2_dot(p, v2_neg(y_axis));
    p = v2_sub(p, x_axis);
    f32 edge2 = v2_dot(p, x_axis);
    p = v2_sub(p, y_axis);
    f32 edge3 = v2_dot(p, y_axis);
    p = v2_add(p, x_axis);
    f32 edge4 = v2_dot(p, v2_neg(x_axis));
    if (edge1 < 0 && edge2 < 0 && edge3 < 0 && edge4 < 0) {
        return 1;
    }

    return 0;
}
