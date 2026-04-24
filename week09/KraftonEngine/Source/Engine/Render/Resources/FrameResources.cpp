// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/Buffers/LightBufferTypes.h"
#include "Render/Resources/FrameResources.h"

#include <cstring>

#include "Core/ResourceTypes.h"
#include "Materials/MaterialManager.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

void FFrameResources::Create(ID3D11Device* InDevice)
{
    FrameBuffer.Create(InDevice, sizeof(FFrameCBData));
    PerObjectConstantBuffer.Create(InDevice, sizeof(FPerObjectCBData));
    GlobalLightBuffer.Create(InDevice, sizeof(FGlobalLightConstants));
    TextBatch.Create(InDevice);

    // s0: LinearClamp sampler for post process and UI.
    {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter             = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU           = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV           = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW           = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.ComparisonFunc     = D3D11_COMPARISON_NEVER;
        desc.MinLOD             = 0;
        desc.MaxLOD             = D3D11_FLOAT32_MAX;
        InDevice->CreateSamplerState(&desc, &LinearClampSampler);
    }

    {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter             = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU           = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV           = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW           = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.ComparisonFunc     = D3D11_COMPARISON_NEVER;
        desc.MinLOD             = 0;
        desc.MaxLOD             = D3D11_FLOAT32_MAX;
        InDevice->CreateSamplerState(&desc, &LinearWrapSampler);
    }

    {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter             = D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU           = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV           = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW           = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.ComparisonFunc     = D3D11_COMPARISON_NEVER;
        desc.MinLOD             = 0;
        desc.MaxLOD             = D3D11_FLOAT32_MAX;
        InDevice->CreateSamplerState(&desc, &PointClampSampler);
    }

    FMaterialManager::Get().Initialize(InDevice);
}

void FFrameResources::Release()
{
    TextBatch.Release();

    for (FConstantBuffer& CB : PerObjectCBPool)
    {
        CB.Release();
    }
    PerObjectCBPool.clear();

    FrameBuffer.Release();
    PerObjectConstantBuffer.Release();
    GlobalLightBuffer.Release();

    if (LocalLightSRV)
    {
        LocalLightSRV->Release();
        LocalLightSRV = nullptr;
    }

    if (LocalLightBuffer)
    {
        LocalLightBuffer->Release();
        LocalLightBuffer = nullptr;
    }

    LocalLightCapacity = 0;

    if (LinearClampSampler)
    {
        LinearClampSampler->Release();
        LinearClampSampler = nullptr;
    }

    if (LinearWrapSampler)
    {
        LinearWrapSampler->Release();
        LinearWrapSampler = nullptr;
    }

    if (PointClampSampler)
    {
        PointClampSampler->Release();
        PointClampSampler = nullptr;
    }
}

void FFrameResources::UpdateLocalLights(ID3D11Device* Device, ID3D11DeviceContext* Context, const TArray<FLocalLightInfo>& Lights)
{
    const uint32 Count = static_cast<uint32>(Lights.size());
    LocalLightCount    = Count;
    if (Count > LocalLightCapacity)
    {
        if (LocalLightSRV)
        {
            LocalLightSRV->Release();
            LocalLightSRV = nullptr;
        }

        if (LocalLightBuffer)
        {
            LocalLightBuffer->Release();
            LocalLightBuffer = nullptr;
        }

        const uint32 NewCapacity = Count < 8u ? 8u : Count;

        D3D11_BUFFER_DESC Desc   = {};
        Desc.ByteWidth           = sizeof(FLocalLightInfo) * NewCapacity;
        Desc.Usage               = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
        Desc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
        Desc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        Desc.StructureByteStride = sizeof(FLocalLightInfo);
        HRESULT hr               = Device->CreateBuffer(&Desc, nullptr, &LocalLightBuffer);
        if (FAILED(hr) || LocalLightBuffer == nullptr)
        {
            MessageBox(nullptr, TEXT("Failed to create LocalLightBuffer"), TEXT("Error"), MB_OK | MB_ICONERROR);
            return;
        }
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format                          = DXGI_FORMAT_UNKNOWN;
        SRVDesc.ViewDimension                   = D3D11_SRV_DIMENSION_BUFFER;
        SRVDesc.Buffer.FirstElement             = 0;
        SRVDesc.Buffer.NumElements              = NewCapacity;
        hr                                      = Device->CreateShaderResourceView(LocalLightBuffer, &SRVDesc, &LocalLightSRV);
        if (FAILED(hr))
        {
            MessageBox(nullptr, TEXT("Failed to create LocalLightSRV"), TEXT("Error"), MB_OK | MB_ICONERROR);
        }
        LocalLightCapacity = NewCapacity;
    }

    if (LocalLightBuffer && Count > 0)
    {
        D3D11_MAPPED_SUBRESOURCE Mapped = {};
        if (SUCCEEDED(Context->Map(LocalLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
        {
            std::memcpy(Mapped.pData, Lights.data(), sizeof(FLocalLightInfo) * Count);
            Context->Unmap(LocalLightBuffer, 0);
        }
    }
}

void FFrameResources::BindSystemSamplers(ID3D11DeviceContext* Ctx)
{
    ID3D11SamplerState* Samplers[3] = { LinearClampSampler, LinearWrapSampler, PointClampSampler };
    Ctx->PSSetSamplers(0, 3, Samplers);
}

void FFrameResources::EnsurePerObjectCBPoolCapacity(ID3D11Device* Device, uint32 RequiredCount)
{
    if (PerObjectCBPool.size() >= RequiredCount)
    {
        return;
    }

    const size_t OldCount = PerObjectCBPool.size();
    PerObjectCBPool.resize(RequiredCount);

    for (size_t Index = OldCount; Index < PerObjectCBPool.size(); ++Index)
    {
        PerObjectCBPool[Index].Create(Device, sizeof(FPerObjectCBData));
    }
}

FConstantBuffer* FFrameResources::GetPerObjectCBForProxy(ID3D11Device* Device, const FPrimitiveSceneProxy& Proxy)
{
    if (Proxy.ProxyId == UINT32_MAX)
    {
        return nullptr;
    }

    EnsurePerObjectCBPoolCapacity(Device, Proxy.ProxyId + 1);
    return &PerObjectCBPool[Proxy.ProxyId];
}

void FFrameResources::EnsureTextCharInfoMap(const FFontResource* Resource)
{
    TextBatch.EnsureCharInfoMap(Resource);
}
