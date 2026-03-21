#pragma once

#include "VertexSimple.h"

static const FVertexSimple triangle_vertices[] =
{
    {  0.0f,  0.0f,  1.0f },
    {  0.0f,  1.0f, -1.0f },
    {  0.0f, -1.0f, -1.0f },
};

static const unsigned short triangle_indices[] =
{
    0, 2, 1,
};

static const unsigned int triangle_vertex_count = 3;
static const unsigned int triangle_index_count = 3;
