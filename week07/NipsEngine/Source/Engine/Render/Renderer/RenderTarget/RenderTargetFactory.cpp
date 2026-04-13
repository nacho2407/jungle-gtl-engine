#include "RenderTargetFactory.h"
#include "RenderTargetBuilder.h"

FRenderTarget FRenderTargetFactory::CreateSceneColor(ID3D11Device* Device, uint32 InWidth, uint32 InHeight)
{
    return FRenderTargetBuilder()
		.SetSize(InWidth, InHeight)
		.SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM)
		.WithRTV()
		.WithSRV()
		.Build(Device);
}


FRenderTarget FRenderTargetFactory::CreateSceneNormal(ID3D11Device* Device, uint32 InWidth, uint32 InHeight)
{
    return FRenderTargetBuilder()
        .SetSize(InWidth, InHeight)
        .SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)
        .WithRTV()
        .WithSRV()
        .Build(Device);
}

FRenderTarget FRenderTargetFactory::CreateSelectionMask(ID3D11Device* Device, uint32 InWidth, uint32 InHeight)
{
    return FRenderTargetBuilder()
        .SetSize(InWidth, InHeight)
        .SetFormat(DXGI_FORMAT_R8_UNORM)
        .WithRTV()
        .WithSRV()
        .Build(Device);
}

FRenderTarget FRenderTargetFactory::CreateSceneLight(ID3D11Device* Device, uint32 InWidth, uint32 InHeight)
{
    return FRenderTargetBuilder()
        .SetSize(InWidth, InHeight)
        .SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)
        .WithRTV()
        .WithSRV()
        .Build(Device);
}

FRenderTarget FRenderTargetFactory::CreateSceneFog(ID3D11Device* Device, uint32 InWidth, uint32 InHeight)
{
    return FRenderTargetBuilder()
        .SetSize(InWidth, InHeight)
        .SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)
        .WithRTV()
        .WithSRV()
        .Build(Device);
}

FRenderTarget FRenderTargetFactory::CreateSceneWorldPos(ID3D11Device* Device, uint32 InWidth, uint32 InHeight)
{
    return FRenderTargetBuilder()
        .SetSize(InWidth, InHeight)
        .SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)  // 더 정확하게 하고 싶으면 DXGI_FORMAT_R32G32B32A32_FLOAT
        .WithRTV()
        .WithSRV()
        .Build(Device);
}

FRenderTarget FRenderTargetFactory::CreateSceneFXAA(ID3D11Device* Device, uint32 InWidth, uint32 InHeight)
{
    return FRenderTargetBuilder()
        .SetSize(InWidth, InHeight)
        .SetFormat(DXGI_FORMAT_B8G8R8A8_UNORM)
        .WithRTV()
        .WithSRV()
        .Build(Device);
}
