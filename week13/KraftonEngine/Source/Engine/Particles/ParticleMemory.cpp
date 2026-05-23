#include "Particles/ParticleMemory.h"
#include "Profiling/Stats/MemoryStats.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace ParticleMemory
{
    size_t AlignSize(size_t Size, size_t InAlignment)
    {
        assert(InAlignment > 0);
        assert((InAlignment & (InAlignment - 1)) == 0);
        return (Size + InAlignment - 1) & ~(InAlignment - 1);
    }

    void* Malloc(size_t Size)
    {
        if (Size == 0)
        {
            return nullptr;
        }

        const size_t AlignedSize = AlignSize(Size);

#if defined(_MSC_VER)
        void* Ptr = _aligned_malloc(AlignedSize, Alignment);
#else
        void* Ptr = std::aligned_alloc(Alignment, AlignedSize);
#endif

        assert(Ptr != nullptr);

        if (Ptr)
        {
            std::memset(Ptr, 0, AlignedSize);
            MemoryStats::OnAllocated(static_cast<uint64>(AlignedSize));
        }

        return Ptr;
    }

    void* Realloc(void* OldPtr, size_t OldSize, size_t NewSize)
    {
        if (NewSize == 0)
        {
            Free(OldPtr, OldSize);
            return nullptr;
        }

        const size_t OldAlignedSize = AlignSize(OldSize);
        const size_t NewAlignedSize = AlignSize(NewSize);

        void* NewPtr = Malloc(NewAlignedSize);
        assert(NewPtr != nullptr);

        if (OldPtr && NewPtr)
        {
            std::memcpy(NewPtr, OldPtr, std::min(OldAlignedSize, NewAlignedSize));
            Free(OldPtr, OldAlignedSize);
        }

        return NewPtr;
    }

    void Free(void* Ptr, size_t Size)
    {
        if (!Ptr)
        {
            return;
        }

        const size_t AlignedSize = AlignSize(Size);

#if defined(_MSC_VER)
        _aligned_free(Ptr);
#else
        std::free(Ptr);
#endif

        MemoryStats::OnDeallocated(static_cast<uint64>(AlignedSize));
    }
}
