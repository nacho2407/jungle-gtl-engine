#pragma once

#include "VertexSimple.h"
#include <cstdint>

static const FVertexSimple plane_vertices[] = {
    {-1.0f, 0.0f, -1.0f}, // 0
    {1.0f, 0.0f, -1.0f},  // 1
    {1.0f, 0.0f, 1.0f},   // 2
    {-1.0f, 0.0f, 1.0f},  // 3
};

static const uint32_t plane_indices[] = {
    0, 2, 1, 0, 3, 2,
};

static const uint32_t plane_vertex_count = sizeof(plane_vertices) / sizeof(FVertexSimple);
static const uint32_t plane_index_count = sizeof(plane_indices) / sizeof(uint32_t);