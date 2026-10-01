#pragma once

#include "Render/RHI/D3D11/Buffers/Buffers.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Submission/Batching/FontBatch.h"

class FPrimitiveProxy;

/*
    Shared frame-scoped GPU resources and batching state.
    Owns common constant buffers, system samplers, local light buffers,
    world text/font batches, and the per-object constant buffer pool.
*/
struct FFrameResources
{
    FConstantBuffer FrameBuffer;
    FConstantBuffer PerObjectConstantBuffer;
    FConstantBuffer GlobalLightBuffer;
    FConstantBuffer ShadowPassBuffer;

    ID3D11SamplerState* LinearClampSampler = nullptr;
    ID3D11SamplerState* LinearWrapSampler  = nullptr;
    ID3D11SamplerState* PointClampSampler  = nullptr;
    ID3D11SamplerState* ShadowSampler      = nullptr;

    ID3D11Buffer*             LocalLightBuffer   = nullptr;
    ID3D11ShaderResourceView* LocalLightSRV      = nullptr;
    uint32                    LocalLightCapacity = 0;
    uint32                    LocalLightCount    = 0;

    ID3D11Buffer*             ForwardDecalDataBuffer       = nullptr;
    ID3D11ShaderResourceView* ForwardDecalDataSRV          = nullptr;
    uint32                    ForwardDecalDataCapacity     = 0;
    uint32                    ForwardDecalDataCount        = 0;
    ID3D11Buffer*             ForwardDecalIndexBuffer      = nullptr;
    ID3D11ShaderResourceView* ForwardDecalIndexSRV         = nullptr;
    uint32                    ForwardDecalIndexCapacity    = 0;
    uint32                    ForwardDecalIndexCount       = 0;

    TArray<FConstantBuffer> PerObjectCBPool;
    FFontBatch              TextBatch;

    void Create(ID3D11Device* InDevice);
    void Release();
    void BindSystemSamplers(ID3D11DeviceContext* Ctx);
    void UpdateLocalLights(ID3D11Device* Device, ID3D11DeviceContext* Context, const TArray<FLocalLightCBData>& Lights);
    void UpdateForwardDecals(
        ID3D11Device* Device,
        ID3D11DeviceContext* Context,
        const TArray<FForwardDecalGPUData>& Decals,
        const TArray<uint32>& DecalIndices);

    void             EnsurePerObjectCBPoolCapacity(ID3D11Device* Device, uint32 RequiredCount);
    FConstantBuffer* GetPerObjectCBForProxy(ID3D11Device* Device, const FPrimitiveProxy& Proxy);
    void             EnsureTextCharInfoMap(const FFontResource* Resource);
};
