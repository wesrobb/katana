#pragma once

#include "gg_types.h"

// Returns 1 if the basis overlap, 0 otherwise.
b8 collider_test(basis_t *a, basis_t *b);

// Returns 1 if the points is contained in the basis, 0 otherwise.
b8 collider_contains_point(basis_t *a, v2 p);
