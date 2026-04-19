#include "LightCullingPass.h"
#include "Core/ResourceManager.h"

namespace {
	const int TileSize = 16;
}

bool FLightCullingPass::Initialize()
{
	return true;
}

bool FLightCullingPass::Release()
{
	return true;
}

bool FLightCullingPass::Begin(const FRenderPassContext* Context)
{
    const FRenderTargetSet* RenderTargets = Context->RenderTargets;

    OutSRV = RenderTargets->SceneColorSRV;
    OutRTV = RenderTargets->SceneColorRTV;

    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr };
    Context->DeviceContext->PSSetShaderResources(4, 3, nullSRVs);

    return true;
}

bool FLightCullingPass::DrawCommand(const FRenderPassContext* Context)
{
	const FRenderTargetSet* RenderTargets = Context->RenderTargets;
	const FRenderBus* RenderBus = Context->RenderBus;
	FRenderResources* Resources = Context->RenderResources;
	ID3D11DeviceContext* DeviceContext = Context->DeviceContext;

	FVector2 ViewportSize = RenderBus->GetViewportSize();

	FComputeShader* CullingCS = FResourceManager::Get().GetComputeShader("Shaders/LightCullingCS.hlsl");
	if (!CullingCS) {
		return false;
	}
	CullingCS->Bind(Context->DeviceContext);

	ID3D11ShaderResourceView* SRVs[] = {
        RenderTargets->SceneDepthSRV,
        Resources->LightStructuredBuffer.GetSRV()
    };
    DeviceContext->CSSetShaderResources(0, 1, &SRVs[0]);
    DeviceContext->CSSetShaderResources(4, 1, &SRVs[1]);

	ID3D11UnorderedAccessView* UAVs[] = {
        Resources->LightCulledIndexBuffer.GetUAV(),
        Resources->LightTileBuffer.GetUAV()
    };
    DeviceContext->CSSetUnorderedAccessViews(0, 2, UAVs, nullptr);

	ID3D11Buffer* b0 = Resources->FrameBuffer.GetBuffer();
	DeviceContext->CSSetConstantBuffers(0, 1, &b0);

	uint32 groupsX = (ViewportSize.X + TileSize - 1) / TileSize;
    uint32 groupsY = (ViewportSize.Y + TileSize - 1) / TileSize;

	CullingCS->Dispatch(Context->DeviceContext, groupsX, groupsY, 1);
    CullingCS->Unbind(Context->DeviceContext);

    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr };
    DeviceContext->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
    DeviceContext->CSSetShaderResources(0, 1, &nullSRVs[0]);
    DeviceContext->CSSetShaderResources(4, 1, &nullSRVs[1]);

	return true;
}

bool FLightCullingPass::End(const FRenderPassContext* Context)
{
    FRenderResources* Resources = Context->RenderResources;
    ID3D11DeviceContext* DeviceContext = Context->DeviceContext;

    ID3D11ShaderResourceView* LightSRVs[] = {
        Resources->LightStructuredBuffer.GetSRV(),
        Resources->LightCulledIndexBuffer.GetSRV(),
        Resources->LightTileBuffer.GetSRV()
    };
    DeviceContext->PSSetShaderResources(4, 3, LightSRVs);

    return true;
}
