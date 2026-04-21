#pragma once

#include "Render/RHI/D3D11/Buffers/Buffers.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Resources/Buffers/ConstantBufferLayouts.h"
#include "Render/Scene/Proxies/Light/LightTypes.h"
#include "Render/Submission/Batching/FontBatch.h"

class FPrimitiveSceneProxy;

/*
    프레임 전체에서 공유하는 GPU/배치 자원 묶음입니다.
    공용 상수 버퍼, 시스템 샘플러, 로컬 라이트 버퍼와 world text/font 배치,
    per-object constant buffer pool을 한 곳에서 관리합니다.
*/
struct FFrameRenderResources
{
    FConstantBuffer FrameBuffer;
    FConstantBuffer PerObjectConstantBuffer;
    FConstantBuffer GlobalLightBuffer;

    ID3D11SamplerState* LinearClampSampler = nullptr;
    ID3D11SamplerState* LinearWrapSampler = nullptr;
    ID3D11SamplerState* PointClampSampler = nullptr;

    ID3D11Buffer* LocalLightBuffer = nullptr;
    ID3D11ShaderResourceView* LocalLightSRV = nullptr;
    uint32 LocalLightCapacity = 0;
    uint32 LocalLightCount = 0;

    TArray<FConstantBuffer> PerObjectCBPool;
    FFontBatch TextBatch;

    void Create(ID3D11Device* InDevice);
    void Release();
    void BindSystemSamplers(ID3D11DeviceContext* Ctx);
    void UpdateLocalLights(ID3D11Device* Device, ID3D11DeviceContext* Context, const TArray<FLocalLightInfo>& Lights);

    void EnsurePerObjectCBPoolCapacity(ID3D11Device* Device, uint32 RequiredCount);
    FConstantBuffer* GetPerObjectCBForProxy(ID3D11Device* Device, const FPrimitiveSceneProxy& Proxy);
    void EnsureTextCharInfoMap(const FFontResource* Resource);
};

using FFrameSharedResources = FFrameRenderResources;
