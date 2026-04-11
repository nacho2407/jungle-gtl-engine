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
