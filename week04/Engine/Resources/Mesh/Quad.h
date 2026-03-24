#pragma once

#include "VertexSimple.h"
#include "MeshPrimitiveTopology.h"
#include <cstdint>

static const FVertexSimple quad_vertices[] = {
    {-1.0f, 0.0f, -1.0f}, // 0
    {1.0f, 0.0f, -1.0f},  // 1
    {-1.0f, 0.0f, 1.0f},  // 2
    {1.0f, 0.0f, 1.0f},   // 3
};

static const uint16_t quad_indices[] = {
    0,
    2,
    1,
    1,
    2,
    3,
};

static constexpr EMeshPrimitiveTopology quad_topology = EMeshPrimitiveTopology::TriangleList;
static const uint32_t quad_vertex_count = sizeof(quad_vertices) / sizeof(FVertexSimple);
static const uint32_t quad_index_count = sizeof(quad_indices) / sizeof(uint16_t);

