#pragma once

#include "Render/D3D11/Resource/Buffer.h"
#include "Render/Core/RenderConstants.h"

struct FFrameSharedResources
{
    FConstantBuffer FrameBuffer;             // b0 — ECBSlot::Frame
    FConstantBuffer PerObjectConstantBuffer; // b1 — ECBSlot::PerObject

    // System Samplers — 프레임 시작 시 s0-s2에 영구 바인딩
    ID3D11SamplerState* LinearClampSampler = nullptr; // s0
    ID3D11SamplerState* LinearWrapSampler = nullptr;  // s1
    ID3D11SamplerState* PointClampSampler = nullptr;  // s2

    void Create(ID3D11Device* InDevice);
    void Release();
    void BindSystemSamplers(ID3D11DeviceContext* Ctx);
};
