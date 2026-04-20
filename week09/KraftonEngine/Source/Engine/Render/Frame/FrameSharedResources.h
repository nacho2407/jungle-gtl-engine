#pragma once

#include "Render/Hardware/Resources/Buffer.h"
#include "Render/Core/RenderConstants.h"

struct FFrameSharedResources
{
    FConstantBuffer FrameBuffer;             // b0 — ECBSlot::Frame
    FConstantBuffer PerObjectConstantBuffer; // b1 — ECBSlot::PerObject
    FConstantBuffer GlobalLightBuffer;       // b4 — ECBSlot::Light (FGlobalLightConstants)

    // System Samplers — 프레임 시작 시 s0-s2에 영구 바인딩
    ID3D11SamplerState* LinearClampSampler = nullptr; // s0
    ID3D11SamplerState* LinearWrapSampler = nullptr;  // s1
    ID3D11SamplerState* PointClampSampler = nullptr;  // s2

    // t6: LocalLights StructuredBuffer — 매 프레임 CollectedLights에서 업로드
    ID3D11Buffer* LocalLightBuffer = nullptr;
    ID3D11ShaderResourceView* LocalLightSRV = nullptr;
    uint32 LocalLightCapacity = 0;

    void Create(ID3D11Device* InDevice);
    void Release();
    void BindSystemSamplers(ID3D11DeviceContext* Ctx);

    // LocalLights StructuredBuffer 업로드 — 용량 초과 시 자동 재생성
    void UpdateLocalLights(ID3D11Device* Device, ID3D11DeviceContext* Context,
                           const TArray<FLocalLightInfo>& Lights);
};
