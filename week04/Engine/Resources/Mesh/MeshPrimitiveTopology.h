#pragma once

#include <cstdint>

enum class EMeshPrimitiveTopology : uint8_t
{
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
};
