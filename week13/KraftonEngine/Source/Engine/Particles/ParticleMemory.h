#pragma once

#include "Core/Types/CoreTypes.h"
#include <cstddef>

namespace ParticleMemory
{
    constexpr size_t Alignment = 16;

    size_t AlignSize(size_t Size, size_t InAlignment = Alignment);

    void* Malloc(size_t Size);
    void* Realloc(void* OldPtr, size_t OldSize, size_t NewSize);
    void  Free(void* Ptr, size_t Size);
}
