#include "LightCullingPass.h"
#include "Core/ResourceManager.h"

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
    Context->DeviceContext->PSSetShaderResources(10, 3, nullSRVs);

    return true;
}

bool FLightCullingPass::DrawCommand(const FRenderPassContext* Context)
{
	const FRenderTargetSet* RenderTargets = Context->RenderTargets;
	const FRenderBus* RenderBus = Context->RenderBus;
	FRenderResources* Resources = Context->RenderResources;
	ID3D11DeviceContext* DeviceContext = Context->DeviceContext;

	FVector2 ViewportSize = RenderBus->GetViewportSize();

	// TODO:: Add hlsl file
	FComputeShader* CullingCS = FResourceManager::Get().GetComputeShader("Shaders/LightCullingCS.hlsl");

	ID3D11ShaderResourceView* SRVs[] = {
        RenderTargets->SceneDepthSRV,
        Resources->LightStructuredBuffer.GetSRV()
    };
    DeviceContext->CSSetShaderResources(0, 1, &SRVs[0]);
    DeviceContext->CSSetShaderResources(10, 1, &SRVs[1]);

	ID3D11UnorderedAccessView* UAVs[] = {
        Resources->LightCulledIndexBuffer.GetUAV(),
        Resources->LightTileBuffer.GetUAV()
    };
    DeviceContext->CSSetUnorderedAccessViews(0, 2, UAVs, nullptr);



	return true;
}

bool FLightCullingPass::End(const FRenderPassContext* Context)
{
	return true;
}
