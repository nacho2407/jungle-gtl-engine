#pragma once

#include "VertexSimple.h"

static const FVertexSimple cube_vertices[] =
{
    { -1.000000f, -1.000000f, -1.000000f },
    { 1.000000f, -1.000000f, -1.000000f },
    { 1.000000f, 1.000000f, -1.000000f },
    { -1.000000f, 1.000000f, -1.000000f },
    { -1.000000f, -1.000000f, 1.000000f },
    { 1.000000f, -1.000000f, 1.000000f },
    { 1.000000f, 1.000000f, 1.000000f },
    { -1.000000f, 1.000000f, 1.000000f },
};

static const unsigned short cube_indices[] =
{
    4, 6, 5, 4, 7, 6, 0, 1, 2, 0, 2, 3,
    0, 7, 4, 0, 3, 7, 1, 6, 2, 1, 5, 6,
    3, 6, 7, 3, 2, 6, 0, 5, 1, 0, 4, 5,
};

static const unsigned int cube_vertex_count = 8;
static const unsigned int cube_index_count = 36;