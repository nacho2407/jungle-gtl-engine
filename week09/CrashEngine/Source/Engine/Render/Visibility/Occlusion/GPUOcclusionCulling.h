// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Profiling/Stats.h"

class FPrimitiveProxy;

// GPU AABB layout must match the AABB struct in Shaders/Passes/Visibility/OcclusionTestPass.hlsl.
struct FGPUOcclusionAABB
{
    float MinX, MinY, MinZ, _pad0;
    float MaxX, MaxY, MaxZ, _pad1;
};

// FGPUOcclusionCulling는 렌더 영역의 핵심 동작을 담당합니다.
class FGPUOcclusionCulling
{
public:
    FGPUOcclusionCulling() = default;
    ~FGPUOcclusionCulling() { Release(); }

    FGPUOcclusionCulling(const FGPUOcclusionCulling&)            = delete;
    FGPUOcclusionCulling& operator=(const FGPUOcclusionCulling&) = delete;

    void Initialize(ID3D11Device* InDevice);
    void Release();
    bool IsInitialized() const { return bInitialized; }

    // Reads the previous frame's staging buffer and updates the occlusion bitset.
    void ReadbackResults(ID3D11DeviceContext* Ctx);

    // Clears all readback and pre-gather state.
    void InvalidateResults();

    // Pre-gathers AABBs during collection so DispatchOcclusionTest can skip gather work.
    void BeginGatherAABB(uint32 ExpectedCount);
    void GatherAABB(FPrimitiveProxy* Proxy);
    void EndGatherAABB();

    // Generates Hi-Z, dispatches the occlusion test, and copies results to staging.
    void DispatchOcclusionTest(
        ID3D11DeviceContext*                 Ctx,
        ID3D11ShaderResourceView*            DepthSRV,
        const TArray<FPrimitiveProxy*>& VisibleProxies,
        const FMatrix& View, const FMatrix& Proj,
        uint32 ViewportWidth, uint32 ViewportHeight);

    bool IsOccluded(const FPrimitiveProxy* Proxy) const;

    bool HasResults() const { return bHasResults; }

private:
    void CreateHiZResources(uint32 Width, uint32 Height);
    void ReleaseHiZResources();
    void CreateBuffers(uint32 ProxyCount);
    void ReleaseBuffers();
    void GenerateHiZ(ID3D11DeviceContext* Ctx, ID3D11ShaderResourceView* DepthSRV, uint32 Width, uint32 Height);
    void UpdateParamsCB(ID3D11DeviceContext* Ctx, const void* Data, uint32 Size);

private:
    ID3D11Device* Device = nullptr;

    // Compute shaders
    ID3D11ComputeShader* HiZCopyCS       = nullptr;
    ID3D11ComputeShader* HiZDownsampleCS = nullptr;
    ID3D11ComputeShader* OcclusionTestCS = nullptr;

    // Hi-Z ping-pong textures: even mips on A, odd mips on B.
    ID3D11Texture2D*                   HiZTextureA = nullptr;
    ID3D11Texture2D*                   HiZTextureB = nullptr;
    ID3D11ShaderResourceView*          HiZSRV_A    = nullptr; // full chain for OcclusionTest
    ID3D11ShaderResourceView*          HiZSRV_B    = nullptr; // full chain for OcclusionTest
    TArray<ID3D11UnorderedAccessView*> HiZUAVs_A;             // per-mip write targets
    TArray<ID3D11UnorderedAccessView*> HiZUAVs_B;
    TArray<ID3D11ShaderResourceView*>  HiZSRVs_A; // per-mip read sources
    TArray<ID3D11ShaderResourceView*>  HiZSRVs_B;
    uint32                             HiZWidth    = 0;
    uint32                             HiZHeight   = 0;
    uint32                             HiZMipCount = 0;

    // AABB StructuredBuffer (SRV)
    ID3D11Buffer*             AABBBuffer         = nullptr;
    ID3D11ShaderResourceView* AABBSRV            = nullptr;
    uint32                    AABBBufferCapacity = 0;

    // Visibility RWStructuredBuffer (UAV) + double-buffered staging readback
    ID3D11Buffer*                       VisibilityBuffer              = nullptr;
    ID3D11UnorderedAccessView*          VisibilityUAV                 = nullptr;
    static constexpr uint32             STAGING_COUNT                 = 3;
    ID3D11Buffer*                       StagingBuffers[STAGING_COUNT] = {};
    TArray<const FPrimitiveProxy*> StagingProxies[STAGING_COUNT];
    uint32                              StagingProxyCount[STAGING_COUNT] = {};
    uint32                              StagingMaxProxyId[STAGING_COUNT] = {};
    uint32                              WriteIndex                       = 0; // Current frame write slot.
    uint32                              VisibilityBufferCapacity         = 0;

    // Constant buffer (shared between HiZ and OcclusionTest passes)
    ID3D11Buffer* ParamsCB = nullptr;

    // CPU-side AABB staging array: gather scattered bounds, then memcpy once.
    TArray<FGPUOcclusionAABB> CPUAABBStaging;

    // Pre-gather state populated by GatherAABB during collection.
    bool   bPreGathered      = false;
    uint32 PreGatherWritePos = 0;
    uint32 PreGatherMaxId    = 0;

    // CPU-side occlusion results: bit array indexed by ProxyId (O(1) lookup).
    TArray<uint32> OccludedBits; // each bit = 1 proxy, OccludedBits[id/32] & (1<<(id%32))
    bool           bHasResults  = false;
    uint32         FrameCount   = 0;
    bool           bInitialized = false;
};
