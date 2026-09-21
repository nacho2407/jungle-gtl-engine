#pragma once
#include "Structs.h"
#include "Math/FVector.h"
#include "Math/FVector4.h"

static FVertexSimple axes_vertices[] =
{
    // X axis - Red
    FVertexSimple(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f),
    FVertexSimple(3.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f),

    // Y axis - Green
    FVertexSimple(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f),
    FVertexSimple(0.0f, 3.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f),

    // Z axis - Blue
    FVertexSimple(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f),
    FVertexSimple(0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 1.0f, 1.0f),
};