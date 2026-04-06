#pragma once

#include "Render/Types/RenderTypes.h"
#include "Core/CoreTypes.h"
#include "Math/Matrix.h"

class FPrimitiveSceneProxy;

// GPU AABB layout — must match OcclusionTest.hlsl AABB struct
struct FGPUOcclusionAABB
{
	float MinX, MinY, MinZ, _pad0;
	float MaxX, MaxY, MaxZ, _pad1;
};

class FGPUOcclusionCulling
{
public:
	FGPUOcclusionCulling() = default;
	~FGPUOcclusionCulling() { Release(); }

	FGPUOcclusionCulling(const FGPUOcclusionCulling&) = delete;
	FGPUOcclusionCulling& operator=(const FGPUOcclusionCulling&) = delete;

	void Initialize(ID3D11Device* InDevice);
	void Release();
	bool IsInitialized() const { return bInitialized; }

	// 이전 프레임 staging buffer → OccludedSet 갱신
	void ReadbackResults(ID3D11DeviceContext* Ctx);

	// 씬 전환 시 댕글링 프록시 포인터 제거
	void InvalidateResults();

	// Hi-Z 생성 + Occlusion Test 디스패치 + staging 복사
	void DispatchOcclusionTest(
		ID3D11DeviceContext* Ctx,
		ID3D11ShaderResourceView* DepthSRV,
		const TArray<FPrimitiveSceneProxy*>& VisibleProxies,
		const FMatrix& View, const FMatrix& Proj,
		uint32 ViewportWidth, uint32 ViewportHeight);

	bool IsOccluded(const FPrimitiveSceneProxy* Proxy) const;

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
	ID3D11ComputeShader* HiZCopyCS = nullptr;
	ID3D11ComputeShader* HiZDownsampleCS = nullptr;
	ID3D11ComputeShader* OcclusionTestCS = nullptr;

	// Hi-Z texture + per-mip views
	ID3D11Texture2D* HiZTexture = nullptr;
	ID3D11ShaderResourceView* HiZSRV = nullptr;
	TArray<ID3D11UnorderedAccessView*> HiZMipUAVs;

	// Ping-pong temp texture — avoids DX11 SRV/UAV same-resource binding hazard
	ID3D11Texture2D* HiZTempTexture = nullptr;
	TArray<ID3D11ShaderResourceView*> HiZTempMipSRVs;
	uint32 HiZWidth = 0;
	uint32 HiZHeight = 0;
	uint32 HiZMipCount = 0;

	// AABB StructuredBuffer (SRV)
	ID3D11Buffer* AABBBuffer = nullptr;
	ID3D11ShaderResourceView* AABBSRV = nullptr;
	uint32 AABBBufferCapacity = 0;

	// Visibility RWStructuredBuffer (UAV) + double-buffered staging readback
	ID3D11Buffer* VisibilityBuffer = nullptr;
	ID3D11UnorderedAccessView* VisibilityUAV = nullptr;
	static constexpr uint32 STAGING_COUNT = 3;
	ID3D11Buffer* StagingBuffers[STAGING_COUNT] = {};
	TArray<const FPrimitiveSceneProxy*> StagingProxies[STAGING_COUNT];
	uint32 StagingProxyCount[STAGING_COUNT] = {};
	uint32 WriteIndex = 0;   // 현재 프레임 기록용
	uint32 VisibilityBufferCapacity = 0;

	// Constant buffer (shared between HiZ and OcclusionTest passes)
	ID3D11Buffer* ParamsCB = nullptr;

	// CPU-side AABB staging array — gather scattered bounds then single memcpy to mapped buffer
	TArray<FGPUOcclusionAABB> CPUAABBStaging;

	// CPU-side occlusion results — bit array indexed by ProxyId (O(1) lookup)
	TArray<uint32> OccludedBits;    // each bit = 1 proxy, OccludedBits[id/32] & (1<<(id%32))
	bool bHasResults = false;
	uint32 FrameCount = 0;
	bool bInitialized = false;
};
