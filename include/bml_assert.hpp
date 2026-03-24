/**
 * @file bml_assert.hpp
 * @brief Debug assertion macro for BML C++ wrappers
 *
 * BML_ASSERT validates preconditions at construction time.
 * Fires in Debug builds; expands to nothing in Release.
 */
#pragma once

#include <cassert>

#define BML_ASSERT(expr) assert(expr)
