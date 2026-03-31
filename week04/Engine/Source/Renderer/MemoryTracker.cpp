#include "Renderer/MemoryTracker.h"

#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Engine/EngineStatics.h"
#include "Renderer/D3D11/D3D11Common.h"

namespace
{
    uint64 GetFormatBytesPerPixel(DXGI_FORMAT Format)
    {
        switch (Format)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
            return 4;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return 16;
        case DXGI_FORMAT_R32G32B32_FLOAT:
            return 12;
        case DXGI_FORMAT_R32G32_FLOAT:
            return 8;
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_D32_FLOAT:
            return 4;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
            return 8;
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
            return 4;
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_D16_UNORM:
            return 2;
        case DXGI_FORMAT_R8_UNORM:
            return 1;
        default:
            return 4;
        }
    }

    // 현재 프로젝트에서 사용하지는 않지만 2D 텍스처에 밉맵 기능이 있어서 추가해둔 헬퍼 함수입니다.
    UINT ResolveMipLevelCount(const D3D11_TEXTURE2D_DESC& Desc)
    {
        if (Desc.MipLevels > 0)
        {
            return Desc.MipLevels;
        }

        UINT Width = std::max(Desc.Width, 1u);
        UINT Height = std::max(Desc.Height, 1u);
        UINT MipLevels = 1;

        while (Width > 1 || Height > 1)
        {
            Width = std::max(Width / 2u, 1u);
            Height = std::max(Height / 2u, 1u);
            MipLevels++;
        }

        return MipLevels;
    }
}

FMemoryTracker GMemoryTracker;

FTrackedTextureAllocation::FTrackedTextureAllocation(FMemoryTracker* InTracker, uint64 InBytes)
    : Tracker(InTracker), Bytes(InBytes)
{
}

FTrackedTextureAllocation::~FTrackedTextureAllocation()
{
    if (Tracker != nullptr && Bytes > 0)
    {
        Tracker->RemoveTextureBytes(Bytes);
    }
}

FTrackedMemoryStats FMemoryTracker::GetTrackedMemoryStats() const
{
    std::lock_guard<std::mutex> Lock(Mutex);
    return TrackedStats;
}

FPlatformMemoryStats FMemoryTracker::QueryPlatformMemoryStats() const
{
    FPlatformMemoryStats Stats = {};

    MEMORYSTATUSEX MemoryStatus = {};
    MemoryStatus.dwLength = sizeof(MemoryStatus);
    if (!GlobalMemoryStatusEx(&MemoryStatus))
    {
        return Stats;
    }

    Stats.TotalPhysical = static_cast<uint64>(MemoryStatus.ullTotalPhys);
    Stats.AvailablePhysical = static_cast<uint64>(MemoryStatus.ullAvailPhys);
    Stats.UsedPhysical = Stats.TotalPhysical - Stats.AvailablePhysical;

    Stats.TotalVirtual = static_cast<uint64>(MemoryStatus.ullTotalVirtual);
    Stats.AvailableVirtual = static_cast<uint64>(MemoryStatus.ullAvailVirtual);
    Stats.UsedVirtual = Stats.TotalVirtual - Stats.AvailableVirtual;

    return Stats;
}

FMemorySnapshot FMemoryTracker::CaptureSnapshot() const
{
    FMemorySnapshot Snapshot;
    Snapshot.Platform = QueryPlatformMemoryStats();
    Snapshot.TrackedGpu = GetTrackedMemoryStats();
    Snapshot.HeapAllocatedBytes = UEngineStatics::TotalAllocatedBytes;
    Snapshot.HeapAllocationCount = UEngineStatics::TotalAllocationCount;
    return Snapshot;
}

void FMemoryTracker::ResetTrackedMemoryStats()
{
    std::lock_guard<std::mutex> Lock(Mutex);
    TrackedStats = {};
}

uint64 FMemoryTracker::EstimateTexture2DSizeBytes(const D3D11_TEXTURE2D_DESC& Desc) const
{
    const uint64 BytesPerPixel = GetFormatBytesPerPixel(Desc.Format);
    uint64 TotalBytes = 0;

    UINT Width = std::max(Desc.Width, 1u);
    UINT Height = std::max(Desc.Height, 1u);
    const UINT MipLevels = ResolveMipLevelCount(Desc);
    const UINT SampleCount = std::max(Desc.SampleDesc.Count, 1u);
    const UINT ArraySize = std::max(Desc.ArraySize, 1u);

    for (UINT MipIndex = 0; MipIndex < MipLevels; ++MipIndex)
    {
        TotalBytes += static_cast<uint64>(Width) * static_cast<uint64>(Height) *
                      static_cast<uint64>(ArraySize) * static_cast<uint64>(SampleCount) *
                      BytesPerPixel;

        Width = std::max(Width / 2u, 1u);
        Height = std::max(Height / 2u, 1u);
    }

    return TotalBytes;
}

FTrackedTextureAllocationHandle FMemoryTracker::TrackTextureAllocation(uint64 Bytes)
{
    if (Bytes == 0)
    {
        return {};
    }

    {
        // 텍스처 생성 시점에 먼저 총량을 올립니다.
        std::lock_guard<std::mutex> Lock(Mutex);
        AddToCounter(TrackedStats.TextureBytes, Bytes);
    }

    return std::make_shared<FTrackedTextureAllocation>(this, Bytes);
}

void FMemoryTracker::AddRenderTargetBytes(uint64 Bytes)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    AddToCounter(TrackedStats.RenderTargetBytes, Bytes);
}

void FMemoryTracker::RemoveRenderTargetBytes(uint64 Bytes)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    RemoveFromCounter(TrackedStats.RenderTargetBytes, Bytes);
}

void FMemoryTracker::AddDepthStencilBytes(uint64 Bytes)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    AddToCounter(TrackedStats.DepthStencilBytes, Bytes);
}

void FMemoryTracker::RemoveDepthStencilBytes(uint64 Bytes)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    RemoveFromCounter(TrackedStats.DepthStencilBytes, Bytes);
}

void FMemoryTracker::AddVertexBufferBytes(uint64 Bytes)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    AddToCounter(TrackedStats.VertexBufferBytes, Bytes);
}

void FMemoryTracker::AddIndexBufferBytes(uint64 Bytes)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    AddToCounter(TrackedStats.IndexBufferBytes, Bytes);
}

void FMemoryTracker::AddVertexShaderBlobBytes(uint64 Bytes)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    AddToCounter(TrackedStats.VertexShaderBlobBytes, Bytes);
}

void FMemoryTracker::AddPixelShaderBlobBytes(uint64 Bytes)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    AddToCounter(TrackedStats.PixelShaderBlobBytes, Bytes);
}

void FMemoryTracker::AddToCounter(uint64& Counter, uint64 Bytes) { Counter += Bytes; }

void FMemoryTracker::RemoveFromCounter(uint64& Counter, uint64 Bytes)
{
    Counter = (Counter >= Bytes) ? (Counter - Bytes) : 0;
}

void FMemoryTracker::RemoveTextureBytes(uint64 Bytes)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    RemoveFromCounter(TrackedStats.TextureBytes, Bytes);
}
