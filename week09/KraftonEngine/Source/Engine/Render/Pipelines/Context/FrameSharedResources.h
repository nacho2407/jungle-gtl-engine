#pragma once

#include "Render/RHI/D3D11/Buffers/Buffers.h"
#include "Render/Resources/BindingSlots.h"
#include "Render/Resources/ConstantBufferLayouts.h"
#include "Render/Types/LightTypes.h"

/*
    프레임 전체에서 공유하는 GPU 자원 묶음입니다.
    공용 상수 버퍼, 시스템 샘플러, 로컬 라이트 버퍼를 한 번만 만들고 각 패스가 재사용합니다.
*/
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
