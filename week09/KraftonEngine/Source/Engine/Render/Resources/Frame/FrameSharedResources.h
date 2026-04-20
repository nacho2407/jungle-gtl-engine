#pragma once

#include "Render/D3D11/Buffers/Buffers.h"
#include "Render/Core/RenderConstants.h"

struct FFrameSharedResources
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

    void Create(ID3D11Device* InDevice);
    void Release();
    void BindSystemSamplers(ID3D11DeviceContext* Ctx);
    void UpdateLocalLights(ID3D11Device* Device, ID3D11DeviceContext* Context, const TArray<FLocalLightInfo>& Lights);
};

using FFrameSharedResources = FFrameSharedResources;
