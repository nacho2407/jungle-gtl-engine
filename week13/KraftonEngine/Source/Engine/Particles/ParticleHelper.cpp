#include "Particles/ParticleHelper.h"
#include "Particles/ParticleMemory.h"

#include <cassert>
#include <cstring>

FParticleDataContainer::~FParticleDataContainer()
{
    Free();
}

FParticleDataContainer::FParticleDataContainer(FParticleDataContainer&& Other) noexcept
{
    MemBlockSize = Other.MemBlockSize;
    ParticleDataNumBytes = Other.ParticleDataNumBytes;
    ParticleIndicesNumShorts = Other.ParticleIndicesNumShorts;
    ParticleData = Other.ParticleData;
    ParticleIndices = Other.ParticleIndices;

    Other.MemBlockSize = 0;
    Other.ParticleDataNumBytes = 0;
    Other.ParticleIndicesNumShorts = 0;
    Other.ParticleData = nullptr;
    Other.ParticleIndices = nullptr;
}

FParticleDataContainer& FParticleDataContainer::operator=(FParticleDataContainer&& Other) noexcept
{
    if (this == &Other)
    {
        return *this;
    }

    Free();

    MemBlockSize = Other.MemBlockSize;
    ParticleDataNumBytes = Other.ParticleDataNumBytes;
    ParticleIndicesNumShorts = Other.ParticleIndicesNumShorts;
    ParticleData = Other.ParticleData;
    ParticleIndices = Other.ParticleIndices;

    Other.MemBlockSize = 0;
    Other.ParticleDataNumBytes = 0;
    Other.ParticleIndicesNumShorts = 0;
    Other.ParticleData = nullptr;
    Other.ParticleIndices = nullptr;

    return *this;
}

void FParticleDataContainer::Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts)
{
    Free();

    assert(InParticleDataNumBytes >= 0);
    assert(InParticleIndicesNumShorts >= 0);

    ParticleDataNumBytes =
        static_cast<int32>(ParticleMemory::AlignSize(static_cast<size_t>(InParticleDataNumBytes)));

    ParticleIndicesNumShorts = InParticleIndicesNumShorts;

    MemBlockSize =
        ParticleDataNumBytes +
        ParticleIndicesNumShorts * static_cast<int32>(sizeof(uint16));

    if (MemBlockSize <= 0)
    {
        return;
    }

    ParticleData = static_cast<uint8*>(
        ParticleMemory::Malloc(static_cast<size_t>(MemBlockSize)));

    assert(ParticleData != nullptr);

    std::memset(ParticleData, 0, static_cast<size_t>(MemBlockSize));

    ParticleIndices =
        reinterpret_cast<uint16*>(ParticleData + ParticleDataNumBytes);
}

void FParticleDataContainer::Free()
{
    if (!ParticleData)
    {
        MemBlockSize = 0;
        ParticleDataNumBytes = 0;
        ParticleIndicesNumShorts = 0;
        ParticleIndices = nullptr;
        return;
    }

    ParticleMemory::Free(ParticleData, static_cast<size_t>(MemBlockSize));

    MemBlockSize = 0;
    ParticleDataNumBytes = 0;
    ParticleIndicesNumShorts = 0;
    ParticleData = nullptr;
    ParticleIndices = nullptr;
}
