#include "Geometry/Cube.h"

FVertexSimple cube_vertices[] =
{
    FVertexSimple(1.0f, 1.0f, 1.0f, 0.759124f, 0.079568f, 0.962616f, 1.0f),
    FVertexSimple(1.0f, 1.0f, -1.0f, 0.759124f, 0.079568f, 0.962616f, 1.0f),
    FVertexSimple(1.0f, -1.0f, 1.0f, 0.759124f, 0.079568f, 0.962616f, 1.0f),
    FVertexSimple(1.0f, -1.0f, -1.0f, 0.759124f, 0.079568f, 0.962616f, 1.0f),
    FVertexSimple(-1.0f, 1.0f, 1.0f, 0.087058f, 0.165636f, 0.962616f, 1.0f),
    FVertexSimple(-1.0f, 1.0f, -1.0f, 0.087058f, 0.165636f, 0.962616f, 1.0f),
    FVertexSimple(-1.0f, -1.0f, 1.0f, 0.087058f, 0.165636f, 0.962616f, 1.0f),
    FVertexSimple(-1.0f, -1.0f, -1.0f, 0.087058f, 0.165636f, 0.962616f, 1.0f),
};

uint32_t cube_indices[] =
{
    // +X
    0, 1, 3,
    0, 3, 2,

    // -X
    4, 6, 7,
    4, 7, 5,

    // +Y
    4, 5, 1,
    4, 1, 0,

    // -Y
    6, 2, 3,
    6, 3, 7,

    // +Z
    4, 0, 2,
    4, 2, 6,

    // -Z
    5, 7, 3,
    5, 3, 1
};

const size_t cube_index_count = sizeof(cube_indices) / sizeof(uint32_t);

const size_t cube_vertex_count = sizeof(cube_vertices) / sizeof(FVertexSimple);
