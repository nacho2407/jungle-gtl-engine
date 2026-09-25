#pragma once

#include "VertexSimple.h"
#include "MeshPrimitiveTopology.h"
#include <cstdint>

static const FVertexSimple cube_vertices[] = {
    {-0.500000f, -0.500000f, -0.500000f}, {0.500000f, -0.500000f, -0.500000f},
    {0.500000f, 0.500000f, -0.500000f},   {-0.500000f, 0.500000f, -0.500000f},
    {-0.500000f, -0.500000f, 0.500000f},  {0.500000f, -0.500000f, 0.500000f},
    {0.500000f, 0.500000f, 0.500000f},    {-0.500000f, 0.500000f, 0.500000f},
};

static const uint16_t cube_indices[] = {
    4, 5, 6,
    4, 6, 7,

    0, 2, 1,
    0, 3, 2,

    0, 4, 7,
    0, 7, 3,

    1, 2, 6,
    1, 6, 5,

    3, 7, 6,
    3, 6, 2,

    0, 1, 5,
    0, 5, 4,
};

static constexpr EMeshPrimitiveTopology cube_topology = EMeshPrimitiveTopology::TriangleList;
static const uint32_t cube_vertex_count = 8;
static const uint32_t cube_index_count = 36;