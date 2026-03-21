#pragma once

#include "../VertexSimple.h"
#include <cstdint>

static const FVertexSimple gizmo_translation_vertices[] = {
    // Shaft box
    {0.000000f, -0.030000f, -0.030000f}, // 0
    {0.700000f, -0.030000f, -0.030000f}, // 1
    {0.700000f, 0.030000f, -0.030000f},  // 2
    {0.000000f, 0.030000f, -0.030000f},  // 3
    {0.000000f, -0.030000f, 0.030000f},  // 4
    {0.700000f, -0.030000f, 0.030000f},  // 5
    {0.700000f, 0.030000f, 0.030000f},   // 6
    {0.000000f, 0.030000f, 0.030000f},   // 7

    // Arrow head base
    {0.700000f, -0.080000f, -0.080000f}, // 8
    {0.700000f, 0.080000f, -0.080000f},  // 9
    {0.700000f, 0.080000f, 0.080000f},   // 10
    {0.700000f, -0.080000f, 0.080000f},  // 11

    // Arrow tip
    {1.000000f, 0.000000f, 0.000000f}, // 12
};

static const uint16_t gizmo_translation_indices[] = {
    // Shaft box
    4,
    6,
    5,
    4,
    7,
    6,
    0,
    1,
    2,
    0,
    2,
    3,
    0,
    7,
    4,
    0,
    3,
    7,
    1,
    6,
    2,
    1,
    5,
    6,
    3,
    6,
    7,
    3,
    2,
    6,
    0,
    5,
    1,
    0,
    4,
    5,

    // Arrow head base quad
    8,
    9,
    10,
    8,
    10,
    11,

    // Arrow head sides
    8,
    12,
    9,
    9,
    12,
    10,
    10,
    12,
    11,
    11,
    12,
    8,
};

static const uint32_t gizmo_translation_vertex_count = 13;
static const uint32_t gizmo_translation_index_count = 54;