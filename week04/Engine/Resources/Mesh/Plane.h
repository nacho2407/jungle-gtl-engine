#pragma once

#include "VertexSimple.h"
#include "MeshPrimitiveTopology.h"
#include <cstdint>

static const FVertexSimple plane_vertices[] = {
    {-1.0f, 0.0f, -1.0f}, // 0
    {1.0f, 0.0f, -1.0f},  // 1
    {-1.0f, 0.0f, 1.0f},  // 2
    {1.0f, 0.0f, 1.0f},   // 3
};

static const uint16_t plane_indices[] = {
    0, 1, 2, 3,
};

static constexpr EMeshPrimitiveTopology plane_topology = EMeshPrimitiveTopology::TriangleStrip;
static const uint32_t plane_vertex_count = sizeof(plane_vertices) / sizeof(FVertexSimple);
static const uint32_t plane_index_count = sizeof(plane_indices) / sizeof(uint16_t);
