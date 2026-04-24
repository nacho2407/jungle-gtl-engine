// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Visibility/Occlusion/GPUOcclusionCulling.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Profiling/Stats.h"

#include <cstring>

// ================================================================
// Helper: compile a compute shader from file
// ================================================================
static ID3D11ComputeShader* CompileCS(ID3D11Device* Dev, const wchar_t* Path, const char* Entry)
{
    ID3DBlob* csBlob  = nullptr;
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
    hr                      = Dev->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &cs);
    csBlob->Release();

    return SUCCEEDED(hr) ? cs : nullptr;
}

// ================================================================
// Initialize / Release
// ================================================================

void FGPUOcclusionCulling::Initialize(ID3D11Device* InDevice)
{
    Device = InDevice;

    HiZCopyCS       = CompileCS(Device, L"Shaders/Passes/Visibility/HiZGeneratePass.hlsl", "CSCopyDepth");
    HiZDownsampleCS = CompileCS(Device, L"Shaders/Passes/Visibility/HiZGeneratePass.hlsl", "CSDownsample");
    OcclusionTestCS = CompileCS(Device, L"Shaders/Passes/Visibility/OcclusionTestPass.hlsl", "CSOcclusionTest");

    if (!HiZCopyCS || !HiZDownsampleCS || !OcclusionTestCS)
    {
        Release();
        return;
    }

    // Constant buffer large enough for the bigger of the two CB structs.
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth         = sizeof(FOcclusionTestParamsCBData);
    cbDesc.Usage             = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&cbDesc, nullptr, &ParamsCB);

    bInitialized = (ParamsCB != nullptr);
}

void FGPUOcclusionCulling::Release()
{
    ReleaseHiZResources();
    ReleaseBuffers();

    if (ParamsCB)
    {
        ParamsCB->Release();
        ParamsCB = nullptr;
    }
    if (HiZCopyCS)
    {
        HiZCopyCS->Release();
        HiZCopyCS = nullptr;
    }
    if (HiZDownsampleCS)
    {
        HiZDownsampleCS->Release();
        HiZDownsampleCS = nullptr;
    }
    if (OcclusionTestCS)
    {
        OcclusionTestCS->Release();
        OcclusionTestCS = nullptr;
    }

    OccludedBits.clear();
    bHasResults = false;
    for (uint32 i = 0; i < STAGING_COUNT; i++)
    {
        StagingProxies[i].clear();
        StagingProxyCount[i] = 0;
    }
    WriteIndex   = 0;
    FrameCount   = 0;
    bInitialized = false;
    Device       = nullptr;
}

void FGPUOcclusionCulling::InvalidateResults()
{
    for (uint32 i = 0; i < STAGING_COUNT; i++)
    {
        StagingProxies[i].clear();
        StagingProxyCount[i] = 0;
        StagingMaxProxyId[i] = 0;
    }
    OccludedBits.clear();
    bHasResults = false;
    WriteIndex  = 0;
    FrameCount  = 0;

    // Reset pre-gather state so BeginGatherAABB can enter normally next frame.
    bPreGathered      = false;
    PreGatherWritePos = 0;
    PreGatherMaxId    = 0;
    CPUAABBStaging.clear();
}

// ================================================================
// Hi-Z resources (recreated on viewport resize)
// ================================================================

void FGPUOcclusionCulling::CreateHiZResources(uint32 Width, uint32 Height)
{
    if (HiZWidth == Width && HiZHeight == Height && HiZTextureA)
        return;

    ReleaseHiZResources();
    HiZWidth  = Width;
    HiZHeight = Height;

    // Mip count = floor(log2(max(w,h))) + 1
    uint32 maxDim = (Width > Height) ? Width : Height;
    HiZMipCount   = 1;
    uint32 tmp    = maxDim;
    while (tmp > 1)
    {
        tmp >>= 1;
        HiZMipCount++;
    }

    // Both textures need SRV and UAV bindings because downsample mips ping-pong.
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width                = Width;
    desc.Height               = Height;
    desc.MipLevels            = HiZMipCount;
    desc.ArraySize            = 1;
    desc.Format               = DXGI_FORMAT_R32_FLOAT;
    desc.SampleDesc.Count     = 1;
    desc.Usage                = D3D11_USAGE_DEFAULT;
    desc.BindFlags            = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    if (FAILED(Device->CreateTexture2D(&desc, nullptr, &HiZTextureA)))
        return;
    if (FAILED(Device->CreateTexture2D(&desc, nullptr, &HiZTextureB)))
        return;

    // Full-chain SRVs used by the occlusion test shader.
    D3D11_SHADER_RESOURCE_VIEW_DESC srvAll = {};
    srvAll.Format                          = DXGI_FORMAT_R32_FLOAT;
    srvAll.ViewDimension                   = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvAll.Texture2D.MipLevels             = HiZMipCount;
    srvAll.Texture2D.MostDetailedMip       = 0;
    Device->CreateShaderResourceView(HiZTextureA, &srvAll, &HiZSRV_A);
    Device->CreateShaderResourceView(HiZTextureB, &srvAll, &HiZSRV_B);

    // Per-mip UAVs + SRVs for both textures
    HiZUAVs_A.resize(HiZMipCount, nullptr);
    HiZUAVs_B.resize(HiZMipCount, nullptr);
    HiZSRVs_A.resize(HiZMipCount, nullptr);
    HiZSRVs_B.resize(HiZMipCount, nullptr);

    for (uint32 i = 0; i < HiZMipCount; i++)
    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format                           = DXGI_FORMAT_R32_FLOAT;
        uavDesc.ViewDimension                    = D3D11_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice               = i;
        Device->CreateUnorderedAccessView(HiZTextureA, &uavDesc, &HiZUAVs_A[i]);
        Device->CreateUnorderedAccessView(HiZTextureB, &uavDesc, &HiZUAVs_B[i]);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                          = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension                   = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels             = 1;
        srvDesc.Texture2D.MostDetailedMip       = i;
        Device->CreateShaderResourceView(HiZTextureA, &srvDesc, &HiZSRVs_A[i]);
        Device->CreateShaderResourceView(HiZTextureB, &srvDesc, &HiZSRVs_B[i]);
    }
}

void FGPUOcclusionCulling::ReleaseHiZResources()
{
    for (auto* v : HiZUAVs_A)
        if (v)
            v->Release();
    for (auto* v : HiZUAVs_B)
        if (v)
            v->Release();
    for (auto* v : HiZSRVs_A)
        if (v)
            v->Release();
    for (auto* v : HiZSRVs_B)
        if (v)
            v->Release();
    HiZUAVs_A.clear();
    HiZUAVs_B.clear();
    HiZSRVs_A.clear();
    HiZSRVs_B.clear();

    if (HiZSRV_A)
    {
        HiZSRV_A->Release();
        HiZSRV_A = nullptr;
    }
    if (HiZSRV_B)
    {
        HiZSRV_B->Release();
        HiZSRV_B = nullptr;
    }
    if (HiZTextureA)
    {
        HiZTextureA->Release();
        HiZTextureA = nullptr;
    }
    if (HiZTextureB)
    {
        HiZTextureB->Release();
        HiZTextureB = nullptr;
    }

    HiZWidth = HiZHeight = HiZMipCount = 0;
}

// ================================================================
// AABB + Visibility buffers (grow-only)
// ================================================================

void FGPUOcclusionCulling::CreateBuffers(uint32 ProxyCount)
{
    if (ProxyCount == 0)
        return;
    if (AABBBufferCapacity >= ProxyCount && AABBBuffer)
        return;

    ReleaseBuffers();

    uint32 Cap               = ProxyCount + (ProxyCount / 4) + 64; // headroom
    AABBBufferCapacity       = Cap;
    VisibilityBufferCapacity = Cap;

    // Upload pre-gathered AABBs when available; otherwise gather visible proxies here.
    {
        D3D11_BUFFER_DESC bd   = {};
        bd.ByteWidth           = Cap * sizeof(FGPUOcclusionAABB);
        bd.Usage               = D3D11_USAGE_DEFAULT;
        bd.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
        bd.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bd.StructureByteStride = sizeof(FGPUOcclusionAABB);
        Device->CreateBuffer(&bd, nullptr, &AABBBuffer);

        D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
        sd.Format                          = DXGI_FORMAT_UNKNOWN;
        sd.ViewDimension                   = D3D11_SRV_DIMENSION_BUFFER;
        sd.Buffer.FirstElement             = 0;
        sd.Buffer.NumElements              = Cap;
        Device->CreateShaderResourceView(AABBBuffer, &sd, &AABBSRV);
    }

    // Visibility RWStructuredBuffer (GPU read/write)
    {
        D3D11_BUFFER_DESC bd   = {};
        bd.ByteWidth           = Cap * sizeof(uint32);
        bd.Usage               = D3D11_USAGE_DEFAULT;
        bd.BindFlags           = D3D11_BIND_UNORDERED_ACCESS;
        bd.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bd.StructureByteStride = sizeof(uint32);
        Device->CreateBuffer(&bd, nullptr, &VisibilityBuffer);

        D3D11_UNORDERED_ACCESS_VIEW_DESC ud = {};
        ud.Format                           = DXGI_FORMAT_UNKNOWN;
        ud.ViewDimension                    = D3D11_UAV_DIMENSION_BUFFER;
        ud.Buffer.FirstElement              = 0;
        ud.Buffer.NumElements               = Cap;
        Device->CreateUnorderedAccessView(VisibilityBuffer, &ud, &VisibilityUAV);
    }

    // Triple-buffered staging (CPU-readable)
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth         = Cap * sizeof(uint32);
        bd.Usage             = D3D11_USAGE_STAGING;
        bd.CPUAccessFlags    = D3D11_CPU_ACCESS_READ;
        for (uint32 i = 0; i < STAGING_COUNT; i++)
            Device->CreateBuffer(&bd, nullptr, &StagingBuffers[i]);
    }
}

void FGPUOcclusionCulling::ReleaseBuffers()
{
    if (AABBSRV)
    {
        AABBSRV->Release();
        AABBSRV = nullptr;
    }
    if (AABBBuffer)
    {
        AABBBuffer->Release();
        AABBBuffer = nullptr;
    }
    if (VisibilityUAV)
    {
        VisibilityUAV->Release();
        VisibilityUAV = nullptr;
    }
    if (VisibilityBuffer)
    {
        VisibilityBuffer->Release();
        VisibilityBuffer = nullptr;
    }
    for (uint32 i = 0; i < STAGING_COUNT; i++)
    {
        if (StagingBuffers[i])
        {
            StagingBuffers[i]->Release();
            StagingBuffers[i] = nullptr;
        }
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
    ID3D11DeviceContext*      Ctx,
    ID3D11ShaderResourceView* DepthSRV,
    uint32 Width, uint32 Height)
{
    SCOPE_STAT_CAT("GenerateHiZ", "4_ExecutePass");
    CreateHiZResources(Width, Height);
    if (!HiZTextureA || HiZMipCount == 0)
        return;

    ID3D11ShaderResourceView*  nullSRV = nullptr;
    ID3D11UnorderedAccessView* nullUAV = nullptr;

    // Mip 0: copy scene depth into TextureA.
    {
        FHiZParamsCBData p = {};
        p.SrcWidth     = Width;
        p.SrcHeight    = Height;
        UpdateParamsCB(Ctx, &p, sizeof(p));

        Ctx->CSSetShader(HiZCopyCS, nullptr, 0);
        Ctx->CSSetConstantBuffers(1, 1, &ParamsCB);
        Ctx->CSSetShaderResources(0, 1, &DepthSRV);
        Ctx->CSSetUnorderedAccessViews(0, 1, &HiZUAVs_A[0], nullptr);

        Ctx->Dispatch((Width + 7) / 8, (Height + 7) / 8, 1);

        Ctx->CSSetShaderResources(0, 1, &nullSRV);
        Ctx->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    }

    // Mip 1+: downsample by ping-ponging between TextureA and TextureB.
    // Even mips use TextureA, odd mips use TextureB.
    Ctx->CSSetShader(HiZDownsampleCS, nullptr, 0);
    uint32 mW = Width, mH = Height;

    for (uint32 mip = 1; mip < HiZMipCount; mip++)
    {
        FHiZParamsCBData p = {};
        p.SrcWidth     = mW;
        p.SrcHeight    = mH;
        UpdateParamsCB(Ctx, &p, sizeof(p));

        uint32 dW = (mW > 1) ? (mW / 2) : 1;
        uint32 dH = (mH > 1) ? (mH / 2) : 1;

        // Source is the previous mip; destination is the opposite texture.
        bool                       bSrcIsA = ((mip - 1) & 1) == 0;
        ID3D11ShaderResourceView*  srcSRV  = bSrcIsA ? HiZSRVs_A[mip - 1] : HiZSRVs_B[mip - 1];
        ID3D11UnorderedAccessView* dstUAV  = bSrcIsA ? HiZUAVs_B[mip] : HiZUAVs_A[mip];

        Ctx->CSSetConstantBuffers(1, 1, &ParamsCB);
        Ctx->CSSetShaderResources(0, 1, &srcSRV);
        Ctx->CSSetUnorderedAccessViews(0, 1, &dstUAV, nullptr);

        Ctx->Dispatch((dW + 7) / 8, (dH + 7) / 8, 1);

        Ctx->CSSetShaderResources(0, 1, &nullSRV);
        Ctx->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

        mW = dW;
        mH = dH;
    }
}

// ================================================================
// Readback: map the staging buffer from the previous frame.
// ================================================================

void FGPUOcclusionCulling::ReadbackResults(ID3D11DeviceContext* Ctx)
{
    uint32 ReadIdx = (WriteIndex + 1) % STAGING_COUNT;

    if (FrameCount < STAGING_COUNT || StagingProxyCount[ReadIdx] == 0 || !StagingBuffers[ReadIdx])
        return;

    SCOPE_STAT_CAT("OcclusionReadback", "1_WorldTick");

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT                  hr = Ctx->Map(StagingBuffers[ReadIdx], 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
        return;

    if (SUCCEEDED(hr))
    {
        const uint32* vis     = static_cast<const uint32*>(mapped.pData);
        const uint32  count   = StagingProxyCount[ReadIdx];
        const auto&   proxies = StagingProxies[ReadIdx];

        uint32 wordCount = (StagingMaxProxyId[ReadIdx] / 32) + 1;
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
// Pre-gather API: collect AABBs during the collect phase instead of GatherLoop.
// ================================================================

void FGPUOcclusionCulling::BeginGatherAABB(uint32 ExpectedCount)
{
    auto& curProxies = StagingProxies[WriteIndex];
    curProxies.resize(ExpectedCount);
    CPUAABBStaging.resize(ExpectedCount);
    PreGatherWritePos = 0;
    PreGatherMaxId    = 0;
    bPreGathered      = false;
}

void FGPUOcclusionCulling::GatherAABB(FPrimitiveProxy* Proxy)
{
    if (!Proxy || Proxy->bNeverCull)
        return;

    auto&  curProxies = StagingProxies[WriteIndex];
    uint32 pos        = PreGatherWritePos;
    curProxies[pos]   = Proxy;
    if (Proxy->ProxyId > PreGatherMaxId)
        PreGatherMaxId = Proxy->ProxyId;
    const FBoundingBox& B = Proxy->CachedBounds;
    CPUAABBStaging[pos]   = { B.Min.X, B.Min.Y, B.Min.Z, 0.0f,
                              B.Max.X, B.Max.Y, B.Max.Z, 0.0f };
    PreGatherWritePos++;
}

void FGPUOcclusionCulling::EndGatherAABB()
{
    StagingProxyCount[WriteIndex] = PreGatherWritePos;
    StagingMaxProxyId[WriteIndex] = PreGatherMaxId;
    bPreGathered                  = true;
}

// ================================================================
// Dispatch: upload AABBs, generate Hi-Z, and run the occlusion test.
// ================================================================

void FGPUOcclusionCulling::DispatchOcclusionTest(
    ID3D11DeviceContext*                 Ctx,
    ID3D11ShaderResourceView*            DepthSRV,
    const TArray<FPrimitiveProxy*>& VisibleProxies,
    const FMatrix& View, const FMatrix& Proj,
    uint32 ViewportWidth, uint32 ViewportHeight)
{
    if (!bInitialized || !DepthSRV)
        return;

    SCOPE_STAT_CAT("OcclusionDispatch", "4_ExecutePass");

    // Upload pre-gathered AABBs when available; otherwise gather visible proxies here.
    {
        SCOPE_STAT_CAT("UploadAABB", "4_ExecutePass");
        if (!bPreGathered)
        {
            // Fallback: gather visible proxy AABBs on demand.
            uint32 visCount   = static_cast<uint32>(VisibleProxies.size());
            auto&  curProxies = StagingProxies[WriteIndex];
            curProxies.resize(visCount);
            CPUAABBStaging.resize(visCount);
            FGPUOcclusionAABB* staging  = CPUAABBStaging.data();
            uint32             writePos = 0;
            uint32             maxId    = 0;

            for (uint32 i = 0; i < visCount; i++)
            {
                FPrimitiveProxy* Proxy = VisibleProxies[i];
                if (!Proxy || Proxy->bNeverCull)
                    continue;

                curProxies[writePos] = Proxy;
                if (Proxy->ProxyId > maxId)
                    maxId = Proxy->ProxyId;
                const FBoundingBox& B = Proxy->CachedBounds;
                staging[writePos]     = { B.Min.X, B.Min.Y, B.Min.Z, 0.0f,
                                          B.Max.X, B.Max.Y, B.Max.Z, 0.0f };
                writePos++;
            }

            StagingProxyCount[WriteIndex] = writePos;
            StagingMaxProxyId[WriteIndex] = maxId;
        }

        bPreGathered = false; // Reset for the next frame.

        uint32 proxyCount = StagingProxyCount[WriteIndex];
        if (proxyCount == 0)
        {
            FrameCount++;
            WriteIndex = (WriteIndex + 1) % STAGING_COUNT;
            return;
        }

        // Ensure GPU buffers
        CreateBuffers(proxyCount);
        if (!AABBBuffer || !VisibilityBuffer)
            return;

        D3D11_BOX dstBox = {};
        dstBox.left      = 0;
        dstBox.right     = proxyCount * sizeof(FGPUOcclusionAABB);
        dstBox.top       = 0;
        dstBox.bottom    = 1;
        dstBox.front     = 0;
        dstBox.back      = 1;
        Ctx->UpdateSubresource(AABBBuffer, 0, &dstBox, CPUAABBStaging.data(), 0, 0);
    }

    uint32 proxyCount = StagingProxyCount[WriteIndex];

    // ?? Step 1: Hi-Z mip chain ??
    GenerateHiZ(Ctx, DepthSRV, ViewportWidth, ViewportHeight);

    // ?? Step 2: Occlusion test dispatch ??
    {
        FOcclusionTestParamsCBData params = {};
        params.ViewProj               = View * Proj;
        params.ViewportWidth          = static_cast<float>(ViewportWidth);
        params.ViewportHeight         = static_cast<float>(ViewportHeight);
        params.NumAABBs               = proxyCount;
        params.HiZMipCount            = HiZMipCount;
        UpdateParamsCB(Ctx, &params, sizeof(params));

        Ctx->CSSetShader(OcclusionTestCS, nullptr, 0);
        Ctx->CSSetConstantBuffers(1, 1, &ParamsCB);

        ID3D11ShaderResourceView* srvs[3] = { AABBSRV, HiZSRV_A, HiZSRV_B };
        Ctx->CSSetShaderResources(0, 3, srvs);
        Ctx->CSSetUnorderedAccessViews(0, 1, &VisibilityUAV, nullptr);

        uint32 groups = (proxyCount + 63) / 64;
        Ctx->Dispatch(groups, 1, 1);

        // Unbind
        ID3D11ShaderResourceView*  nullSRVs[3] = { nullptr, nullptr, nullptr };
        ID3D11UnorderedAccessView* nullUAV     = nullptr;
        Ctx->CSSetShaderResources(0, 3, nullSRVs);
        Ctx->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
        Ctx->CSSetShader(nullptr, nullptr, 0);
    }

    // ?? Step 3: Copy to this frame's staging buffer ??
    Ctx->CopyResource(StagingBuffers[WriteIndex], VisibilityBuffer);

    // Advance write index for next frame
    WriteIndex = (WriteIndex + 1) % STAGING_COUNT;
    FrameCount++;
}

// ================================================================
// IsOccluded: O(1) bit-array lookup by ProxyId.
// ================================================================

bool FGPUOcclusionCulling::IsOccluded(const FPrimitiveProxy* Proxy) const
{
    uint32 id   = Proxy->ProxyId;
    uint32 word = id >> 5;
    if (word >= static_cast<uint32>(OccludedBits.size()))
        return false;
    return (OccludedBits[word] & (1u << (id & 31))) != 0;
}
