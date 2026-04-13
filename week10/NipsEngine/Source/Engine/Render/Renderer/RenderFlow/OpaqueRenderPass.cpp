#include "OpaqueRenderPass.h"
#include "Render/Device/D3DDevice.h"

bool FOpaqueRenderPass::Initialize()
{
    return false;
}

bool FOpaqueRenderPass::Begin(const FRenderPassContext* Context)
{
    const FRenderTargetSet* RenderTargets = Context->RenderTargets;
    ID3D11RenderTargetView* RTVs[3] = { 
		RenderTargets->SceneColorRTV, 
		RenderTargets->SceneNormalRTV,
		RenderTargets->SceneWorldPosRTV
	};
    ID3D11DepthStencilView* DSV = RenderTargets->DepthStencilView;

	Context->DeviceContext->OMSetRenderTargets(ARRAYSIZE(RTVs), RTVs, DSV);


    return false;
}

bool FOpaqueRenderPass::Render(const FRenderPassContext* Context)
{
    return false;
}

bool FOpaqueRenderPass::End(const FRenderPassContext* Context)
{
    return false;
}

bool FOpaqueRenderPass::Release()
{
    return false;
}
