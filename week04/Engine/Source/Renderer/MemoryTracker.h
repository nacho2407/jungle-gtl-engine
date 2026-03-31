#pragma once

#include <memory>
#include <mutex>

#include "Core/HAL/PlatformMemory.h"
#include "Renderer/TrackedMemoryStats.h"

struct D3D11_TEXTURE2D_DESC;

class FMemoryTracker;

/**
 * @brief 텍스처 1개의 추적 수명을 표현하는 토큰 객체입니다.
 *
 * - FTextureResource는 단순 포인터처럼만 쓰이지 않고 값 복사될 수 있습니다.
 *   예를 들어, Font/SubUV atlas 쪽은 캐시된 FTextureResource를 다른 구조체 내부 Atlas 필드로 복사합니다.
 *   이때 FTextureResource 자체에 소멸자를 두고 매번 TextureBytes를 차감하면,
 *   복사본이 여러 개 생긴 만큼 메모리를 여러 번 빼는 문제가 생깁니다.
 *
 * - 텍스처 생성 시 TrackTextureAllocation()이 TextureBytes를 1번 증가시킵니다.
 * - 그와 동시에 FTrackedTextureAllocation shared_ptr를 하나 만듭니다.
 * - FTextureResource가 복사되어도 이 토큰은 shared_ptr로 함께 공유됩니다.
 * - 마지막 참조가 사라질 때에만 이 객체의 소멸자가 실행되고, 그때 TextureBytes를 1번 차감합니다.
 */
struct ENGINE_API FTrackedTextureAllocation
{
    FTrackedTextureAllocation() = default;
    FTrackedTextureAllocation(FMemoryTracker* InTracker, uint64 InBytes);
    ~FTrackedTextureAllocation();

    FTrackedTextureAllocation(const FTrackedTextureAllocation&) = delete;
    FTrackedTextureAllocation& operator=(const FTrackedTextureAllocation&) = delete;

  private:
    FMemoryTracker* Tracker = nullptr;
    uint64          Bytes = 0;
};

using FTrackedTextureAllocationHandle = std::shared_ptr<FTrackedTextureAllocation>;

/**
 * @brief UI나 디버그 오버레이에서 바로 사용하기 쉽도록 묶어둔 메모리 스냅샷 구조체입니다.
 *
 * - Platform: OS가 알려주는 현재 시스템 메모리 상태
 * - TrackedGpu: 엔진이 직접 누적/차감하며 관리하는 GPU 메모리 근사치
 * - HeapAllocatedBytes / HeapAllocationCount: UObject 계열 할당 추적 값
 */
struct ENGINE_API FMemorySnapshot
{
    uint64               HeapAllocatedBytes = 0;
    uint64               HeapAllocationCount = 0;
    FPlatformMemoryStats Platform;
    FTrackedMemoryStats  TrackedGpu;
};

/**
 * @brief 엔진 전역 메모리 추적기입니다.
 */
class ENGINE_API FMemoryTracker
{
  public:
    FTrackedMemoryStats  GetTrackedMemoryStats() const;
    FPlatformMemoryStats QueryPlatformMemoryStats() const;
    FMemorySnapshot      CaptureSnapshot() const;

    void ResetTrackedMemoryStats();

    uint64 EstimateTexture2DSizeBytes(const D3D11_TEXTURE2D_DESC& Desc) const;

    FTrackedTextureAllocationHandle TrackTextureAllocation(uint64 Bytes);

    void AddRenderTargetBytes(uint64 Bytes);
    void RemoveRenderTargetBytes(uint64 Bytes);
    void AddDepthStencilBytes(uint64 Bytes);
    void RemoveDepthStencilBytes(uint64 Bytes);
    void AddVertexBufferBytes(uint64 Bytes);
    void AddIndexBufferBytes(uint64 Bytes);
    void AddVertexShaderBlobBytes(uint64 Bytes);
    void AddPixelShaderBlobBytes(uint64 Bytes);

  private:
    void AddToCounter(uint64& Counter, uint64 Bytes);
    void RemoveFromCounter(uint64& Counter, uint64 Bytes);
    void RemoveTextureBytes(uint64 Bytes);

  private:
    // 여러 곳에서 수정될 수 있는 누적 카운터이므로, 동시 접근을 막기 위해 mutex로 보호합니다.
    mutable std::mutex  Mutex;
    FTrackedMemoryStats TrackedStats;

    friend struct FTrackedTextureAllocation;
};

extern ENGINE_API FMemoryTracker GMemoryTracker;
