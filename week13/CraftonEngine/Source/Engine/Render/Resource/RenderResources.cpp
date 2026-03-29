#include "RenderResources.h"

void FRenderResources::Create(ID3D11Device* InDevice)
{
	FrameBuffer.Create(InDevice, sizeof(FFrameConstants));
	PerObjectConstantBuffer.Create(InDevice, sizeof(FPerObjectConstants));
}

void FRenderResources::Release()
{
	FrameBuffer.Release();
	PerObjectConstantBuffer.Release();
}
