#pragma once

#include "VertexSimple.h"
#include "MeshPrimitiveTopology.h"
#include <cstdint>

static const FVertexSimple triangle_vertices[] = {
    {0.0f, 0.0f, 1.0f},
    {0.0f, 1.0f, -1.0f},
    {0.0f, -1.0f, -1.0f},
};

static const uint16_t triangle_indices[] = {
    0,
    1,
    2,
};

static constexpr EMeshPrimitiveTopology triangle_topology = EMeshPrimitiveTopology::TriangleList;
static const uint32_t triangle_vertex_count = 3;
static const uint32_t triangle_index_count = 3;
