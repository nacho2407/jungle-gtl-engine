#include "GPUOcclusionCulling.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Profiling/Stats.h"

#include <cstring>

// ── CB layouts matching HLSL ──

struct FHiZParamsCB
{
	uint32 SrcWidth;
	uint32 SrcHeight;
	uint32 _pad[2];
};

struct alignas(16) FOcclusionTestParamsCB
{
	FMatrix ViewProj;        // 64 bytes
	float   ViewportWidth;   // 4
	float   ViewportHeight;  // 4
	uint32  NumAABBs;        // 4
	uint32  HiZMipCount;     // 4
};

// ================================================================
// Helper: compile a compute shader from file
// ================================================================
static ID3D11ComputeShader* CompileCS(ID3D11Device* Dev, const wchar_t* Path, const char* Entry)
{
	ID3DBlob* csBlob = nullptr;
	ID3DBlob* errBlob = nullptr;

	HRESULT hr = D3DCompileFromFile(Path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		Entry, "cs_5_0", 0, 0, &csBlob, &errBlob);

	if (FAILED(hr))
	{
		if (errBlob)
		{
			MessageBoxA(nullptr, (char*)errBlob->GetBufferPointer(),
				"Compute Shader Compile Error", MB_OK | MB_ICONERROR);
			errBlob->Release();
		}
		return nullptr;
	}

	ID3D11ComputeShader* cs = nullptr;
	hr = Dev->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &cs);
	csBlob->Release();

	return SUCCEEDED(hr) ? cs : nullptr;
}

// ================================================================
// Initialize / Release
// ================================================================

void FGPUOcclusionCulling::Initialize(ID3D11Device* InDevice)
{
	Device = InDevice;

	HiZCopyCS       = CompileCS(Device, L"Shaders/HiZGenerate.hlsl",  "CSCopyDepth");
	HiZDownsampleCS = CompileCS(Device, L"Shaders/HiZGenerate.hlsl",  "CSDownsample");
	OcclusionTestCS = CompileCS(Device, L"Shaders/OcclusionTest.hlsl", "CSOcclusionTest");

	if (!HiZCopyCS || !HiZDownsampleCS || !OcclusionTestCS)
	{
		Release();
		return;
	}

	// Constant buffer — large enough for the bigger of the two CB structs
	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = sizeof(FOcclusionTestParamsCB);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Device->CreateBuffer(&cbDesc, nullptr, &ParamsCB);

	bInitialized = (ParamsCB != nullptr);
}

void FGPUOcclusionCulling::Release()
{
	ReleaseHiZResources();
	ReleaseBuffers();

	if (ParamsCB)        { ParamsCB->Release();        ParamsCB = nullptr; }
	if (HiZCopyCS)       { HiZCopyCS->Release();       HiZCopyCS = nullptr; }
	if (HiZDownsampleCS) { HiZDownsampleCS->Release(); HiZDownsampleCS = nullptr; }
	if (OcclusionTestCS) { OcclusionTestCS->Release();  OcclusionTestCS = nullptr; }

	OccludedBits.clear();
	bHasResults = false;
	for (uint32 i = 0; i < STAGING_COUNT; i++)
	{
		StagingProxies[i].clear();
		StagingProxyCount[i] = 0;
	}
	WriteIndex = 0;
	FrameCount = 0;
	bInitialized = false;
	Device = nullptr;
}

// ================================================================
// Hi-Z resources (recreated on viewport resize)
// ================================================================

void FGPUOcclusionCulling::CreateHiZResources(uint32 Width, uint32 Height)
{
	if (HiZWidth == Width && HiZHeight == Height && HiZTexture)
		return;

	ReleaseHiZResources();
	HiZWidth  = Width;
	HiZHeight = Height;

	// Mip count = floor(log2(max(w,h))) + 1
	uint32 maxDim = (Width > Height) ? Width : Height;
	HiZMipCount = 1;
	uint32 tmp = maxDim;
	while (tmp > 1) { tmp >>= 1; HiZMipCount++; }

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width            = Width;
	desc.Height           = Height;
	desc.MipLevels        = HiZMipCount;
	desc.ArraySize        = 1;
	desc.Format           = DXGI_FORMAT_R32_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.Usage            = D3D11_USAGE_DEFAULT;
	desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	if (FAILED(Device->CreateTexture2D(&desc, nullptr, &HiZTexture)))
		return;

	// SRV for entire mip chain (used by OcclusionTest)
	D3D11_SHADER_RESOURCE_VIEW_DESC srvAll = {};
	srvAll.Format                    = DXGI_FORMAT_R32_FLOAT;
	srvAll.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvAll.Texture2D.MipLevels       = HiZMipCount;
	srvAll.Texture2D.MostDetailedMip = 0;
	Device->CreateShaderResourceView(HiZTexture, &srvAll, &HiZSRV);

	// Per-mip UAVs on HiZTexture (write target)
	HiZMipUAVs.resize(HiZMipCount, nullptr);
	for (uint32 i = 0; i < HiZMipCount; i++)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC mu = {};
		mu.Format               = DXGI_FORMAT_R32_FLOAT;
		mu.ViewDimension        = D3D11_UAV_DIMENSION_TEXTURE2D;
		mu.Texture2D.MipSlice   = i;
		Device->CreateUnorderedAccessView(HiZTexture, &mu, &HiZMipUAVs[i]);
	}

	// Temp texture for ping-pong reads (avoids SRV/UAV conflict on same resource)
	D3D11_TEXTURE2D_DESC tempDesc = desc;
	tempDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;  // read-only
	if (FAILED(Device->CreateTexture2D(&tempDesc, nullptr, &HiZTempTexture)))
		return;

	HiZTempMipSRVs.resize(HiZMipCount, nullptr);
	for (uint32 i = 0; i < HiZMipCount; i++)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC ms = {};
		ms.Format                    = DXGI_FORMAT_R32_FLOAT;
		ms.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
		ms.Texture2D.MipLevels       = 1;
		ms.Texture2D.MostDetailedMip = i;
		Device->CreateShaderResourceView(HiZTempTexture, &ms, &HiZTempMipSRVs[i]);
	}
}

void FGPUOcclusionCulling::ReleaseHiZResources()
{
	for (auto* v : HiZMipUAVs) if (v) v->Release();
	for (auto* v : HiZTempMipSRVs) if (v) v->Release();
	HiZMipUAVs.clear();
	HiZTempMipSRVs.clear();

	if (HiZSRV)         { HiZSRV->Release();         HiZSRV = nullptr; }
	if (HiZTexture)     { HiZTexture->Release();      HiZTexture = nullptr; }
	if (HiZTempTexture) { HiZTempTexture->Release();  HiZTempTexture = nullptr; }

	HiZWidth = HiZHeight = HiZMipCount = 0;
}

// ================================================================
// AABB + Visibility buffers (grow-only)
// ================================================================

void FGPUOcclusionCulling::CreateBuffers(uint32 ProxyCount)
{
	if (ProxyCount == 0) return;
	if (AABBBufferCapacity >= ProxyCount && AABBBuffer) return;

	ReleaseBuffers();

	uint32 Cap = ProxyCount + (ProxyCount / 4) + 64; // headroom
	AABBBufferCapacity       = Cap;
	VisibilityBufferCapacity = Cap;

	// AABB StructuredBuffer (DEFAULT — updated via UpdateSubresource)
	{
		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth           = Cap * sizeof(FGPUOcclusionAABB);
		bd.Usage               = D3D11_USAGE_DEFAULT;
		bd.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
		bd.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bd.StructureByteStride = sizeof(FGPUOcclusionAABB);
		Device->CreateBuffer(&bd, nullptr, &AABBBuffer);

		D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
		sd.Format              = DXGI_FORMAT_UNKNOWN;
		sd.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
		sd.Buffer.FirstElement = 0;
		sd.Buffer.NumElements  = Cap;
		Device->CreateShaderResourceView(AABBBuffer, &sd, &AABBSRV);
	}

	// Visibility RWStructuredBuffer (GPU read/write)
	{
		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth           = Cap * sizeof(uint32);
		bd.Usage               = D3D11_USAGE_DEFAULT;
		bd.BindFlags           = D3D11_BIND_UNORDERED_ACCESS;
		bd.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bd.StructureByteStride = sizeof(uint32);
		Device->CreateBuffer(&bd, nullptr, &VisibilityBuffer);

		D3D11_UNORDERED_ACCESS_VIEW_DESC ud = {};
		ud.Format              = DXGI_FORMAT_UNKNOWN;
		ud.ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
		ud.Buffer.FirstElement = 0;
		ud.Buffer.NumElements  = Cap;
		Device->CreateUnorderedAccessView(VisibilityBuffer, &ud, &VisibilityUAV);
	}

	// Double-buffered staging (CPU-readable)
	{
		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth      = Cap * sizeof(uint32);
		bd.Usage          = D3D11_USAGE_STAGING;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		for (uint32 i = 0; i < STAGING_COUNT; i++)
			Device->CreateBuffer(&bd, nullptr, &StagingBuffers[i]);
	}
}

void FGPUOcclusionCulling::ReleaseBuffers()
{
	if (AABBSRV)          { AABBSRV->Release();          AABBSRV = nullptr; }
	if (AABBBuffer)       { AABBBuffer->Release();       AABBBuffer = nullptr; }
	if (VisibilityUAV)    { VisibilityUAV->Release();    VisibilityUAV = nullptr; }
	if (VisibilityBuffer) { VisibilityBuffer->Release(); VisibilityBuffer = nullptr; }
	for (uint32 i = 0; i < STAGING_COUNT; i++)
	{
		if (StagingBuffers[i]) { StagingBuffers[i]->Release(); StagingBuffers[i] = nullptr; }
		StagingProxies[i].clear();
		StagingProxyCount[i] = 0;
	}
	AABBBufferCapacity = VisibilityBufferCapacity = 0;
}

// ================================================================
// CB update helper
// ================================================================

void FGPUOcclusionCulling::UpdateParamsCB(ID3D11DeviceContext* Ctx, const void* Data, uint32 Size)
{
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(Ctx->Map(ParamsCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, Data, Size);
		Ctx->Unmap(ParamsCB, 0);
	}
}

// ================================================================
// Hi-Z mip chain generation
// ================================================================

void FGPUOcclusionCulling::GenerateHiZ(
	ID3D11DeviceContext* Ctx,
	ID3D11ShaderResourceView* DepthSRV,
	uint32 Width, uint32 Height)
{
	SCOPE_STAT_CAT("GenerateHiZ", "4_ExecutePass");
	CreateHiZResources(Width, Height);
	if (!HiZTexture || HiZMipCount == 0) return;

	ID3D11ShaderResourceView*  nullSRV = nullptr;
	ID3D11UnorderedAccessView* nullUAV = nullptr;

	// ── Mip 0: copy depth ──
	{
		FHiZParamsCB p = {};
		p.SrcWidth  = Width;
		p.SrcHeight = Height;
		UpdateParamsCB(Ctx, &p, sizeof(p));

		Ctx->CSSetShader(HiZCopyCS, nullptr, 0);
		Ctx->CSSetConstantBuffers(0, 1, &ParamsCB);
		Ctx->CSSetShaderResources(0, 1, &DepthSRV);
		Ctx->CSSetUnorderedAccessViews(0, 1, &HiZMipUAVs[0], nullptr);

		Ctx->Dispatch((Width + 7) / 8, (Height + 7) / 8, 1);

		Ctx->CSSetShaderResources(0, 1, &nullSRV);
		Ctx->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	}

	// Copy mip 0 from HiZTexture → HiZTempTexture so we can read it as SRV
	Ctx->CopySubresourceRegion(HiZTempTexture, 0, 0, 0, 0, HiZTexture, 0, nullptr);

	// ── Mip 1+: max-downsample (ping-pong to avoid SRV/UAV conflict) ──
	Ctx->CSSetShader(HiZDownsampleCS, nullptr, 0);
	uint32 mW = Width, mH = Height;

	for (uint32 mip = 1; mip < HiZMipCount; mip++)
	{
		FHiZParamsCB p = {};
		p.SrcWidth  = mW;
		p.SrcHeight = mH;
		UpdateParamsCB(Ctx, &p, sizeof(p));

		uint32 dW = (mW > 1) ? (mW / 2) : 1;
		uint32 dH = (mH > 1) ? (mH / 2) : 1;

		// Read from TempTexture (SRV), write to HiZTexture (UAV) — different resources
		Ctx->CSSetConstantBuffers(0, 1, &ParamsCB);
		Ctx->CSSetShaderResources(0, 1, &HiZTempMipSRVs[mip - 1]);
		Ctx->CSSetUnorderedAccessViews(0, 1, &HiZMipUAVs[mip], nullptr);

		Ctx->Dispatch((dW + 7) / 8, (dH + 7) / 8, 1);

		Ctx->CSSetShaderResources(0, 1, &nullSRV);
		Ctx->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

		// Copy newly written mip to TempTexture for next iteration's read
		Ctx->CopySubresourceRegion(HiZTempTexture, mip, 0, 0, 0, HiZTexture, mip, nullptr);

		mW = dW;
		mH = dH;
	}
}

// ================================================================
// Readback — map staging buffer from previous frame
// ================================================================

void FGPUOcclusionCulling::ReadbackResults(ID3D11DeviceContext* Ctx)
{
	// 읽기 대상 = 현재 Write 반대편 (2프��임 전 데이터)
	uint32 ReadIdx = (WriteIndex + 1) % STAGING_COUNT;

	if (FrameCount < STAGING_COUNT || StagingProxyCount[ReadIdx] == 0 || !StagingBuffers[ReadIdx])
		return;

	SCOPE_STAT_CAT("OcclusionReadback", "1_WorldTick");

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = Ctx->Map(StagingBuffers[ReadIdx], 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
	if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
		return;  // 아직 미완료 — 기존 OccludedSet 유지

	if (SUCCEEDED(hr))
	{
		const uint32* vis = static_cast<const uint32*>(mapped.pData);
		const uint32 count = StagingProxyCount[ReadIdx];
		const auto& proxies = StagingProxies[ReadIdx];

		// Find max ProxyId to size the bit array
		uint32 maxId = 0;
		for (uint32 i = 0; i < count; i++)
		{
			uint32 id = proxies[i]->ProxyId;
			if (id > maxId) maxId = id;
		}

		uint32 wordCount = (maxId / 32) + 1;
		OccludedBits.resize(wordCount);
		memset(OccludedBits.data(), 0, wordCount * sizeof(uint32));

		for (uint32 i = 0; i < count; i++)
		{
			if (vis[i] == 0)
			{
				uint32 id = proxies[i]->ProxyId;
				OccludedBits[id >> 5] |= (1u << (id & 31));
			}
		}

		bHasResults = true;
		Ctx->Unmap(StagingBuffers[ReadIdx], 0);
	}
}

// ================================================================
// Dispatch — upload AABBs, generate Hi-Z, run occlusion test
// ================================================================

void FGPUOcclusionCulling::DispatchOcclusionTest(
	ID3D11DeviceContext* Ctx,
	ID3D11ShaderResourceView* DepthSRV,
	const TArray<FPrimitiveSceneProxy*>& VisibleProxies,
	const FMatrix& View, const FMatrix& Proj,
	uint32 ViewportWidth, uint32 ViewportHeight)
{
	if (!bInitialized || !DepthSRV) return;

	SCOPE_STAT_CAT("OcclusionDispatch", "4_ExecutePass");

	// Single-pass: filter proxies + gather AABBs simultaneously
	// Avoids double iteration and improves cache locality
	{
		SCOPE_STAT_CAT("UploadAABB", "4_ExecutePass");

		auto& curProxies = StagingProxies[WriteIndex];
		curProxies.clear();

		uint32 visCount = static_cast<uint32>(VisibleProxies.size());
		CPUAABBStaging.resize(visCount); // upper bound
		FGPUOcclusionAABB* staging = CPUAABBStaging.data();
		uint32 writePos = 0;

		for (uint32 i = 0; i < visCount; i++)
		{
			FPrimitiveSceneProxy* Proxy = VisibleProxies[i];
			if (!Proxy || Proxy->bNeverCull) continue;

			curProxies.push_back(Proxy);
			const FBoundingBox& B = Proxy->CachedBounds;
			staging[writePos] = { B.Min.X, B.Min.Y, B.Min.Z, 0.0f,
			                      B.Max.X, B.Max.Y, B.Max.Z, 0.0f };
			writePos++;
		}

		uint32 proxyCount = writePos;
		StagingProxyCount[WriteIndex] = proxyCount;
		if (proxyCount == 0) { FrameCount++; WriteIndex = (WriteIndex + 1) % STAGING_COUNT; return; }

		// Ensure GPU buffers
		CreateBuffers(proxyCount);
		if (!AABBBuffer || !VisibilityBuffer) return;

		D3D11_BOX dstBox = {};
		dstBox.left   = 0;
		dstBox.right  = proxyCount * sizeof(FGPUOcclusionAABB);
		dstBox.top    = 0;
		dstBox.bottom = 1;
		dstBox.front  = 0;
		dstBox.back   = 1;
		Ctx->UpdateSubresource(AABBBuffer, 0, &dstBox, staging, 0, 0);
	}

	uint32 proxyCount = StagingProxyCount[WriteIndex];

	// ── Step 1: Hi-Z mip chain ──
	GenerateHiZ(Ctx, DepthSRV, ViewportWidth, ViewportHeight);

	// ── Step 2: Occlusion test dispatch ──
	{
		FOcclusionTestParamsCB params = {};
		params.ViewProj       = View * Proj;
		params.ViewportWidth  = static_cast<float>(ViewportWidth);
		params.ViewportHeight = static_cast<float>(ViewportHeight);
		params.NumAABBs       = proxyCount;
		params.HiZMipCount    = HiZMipCount;
		UpdateParamsCB(Ctx, &params, sizeof(params));

		Ctx->CSSetShader(OcclusionTestCS, nullptr, 0);
		Ctx->CSSetConstantBuffers(0, 1, &ParamsCB);

		ID3D11ShaderResourceView* srvs[2] = { AABBSRV, HiZSRV };
		Ctx->CSSetShaderResources(0, 2, srvs);
		Ctx->CSSetUnorderedAccessViews(0, 1, &VisibilityUAV, nullptr);

		uint32 groups = (proxyCount + 63) / 64;
		Ctx->Dispatch(groups, 1, 1);

		// Unbind
		ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
		ID3D11UnorderedAccessView* nullUAV = nullptr;
		Ctx->CSSetShaderResources(0, 2, nullSRVs);
		Ctx->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
		Ctx->CSSetShader(nullptr, nullptr, 0);
	}

	// ── Step 3: Copy to this frame's staging buffer ──
	Ctx->CopyResource(StagingBuffers[WriteIndex], VisibilityBuffer);

	// Advance write index for next frame
	WriteIndex = (WriteIndex + 1) % STAGING_COUNT;
	FrameCount++;
}

// ================================================================
// IsOccluded — O(1) bit array lookup by ProxyId
// ================================================================

bool FGPUOcclusionCulling::IsOccluded(const FPrimitiveSceneProxy* Proxy) const
{
	uint32 id = Proxy->ProxyId;
	uint32 word = id >> 5;
	if (word >= static_cast<uint32>(OccludedBits.size()))
		return false;
	return (OccludedBits[word] & (1u << (id & 31))) != 0;
}
