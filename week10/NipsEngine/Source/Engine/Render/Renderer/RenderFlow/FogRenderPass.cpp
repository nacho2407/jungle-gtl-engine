#include "FogRenderPass.h"
#include "Core/ResourceManager.h"

bool FFogRenderPass::Initialize()
{
    return true;
}

bool FFogRenderPass::Release()
{
    return true;
}

bool FFogRenderPass::Begin(const FRenderPassContext* Context)
{
	// Fog 부분은 다른 로직으로 교체할 예정이라 임시로 넣음
    OutSRV = Context->RenderTargets->SceneLightSRV;
    OutRTV = Context->RenderTargets->SceneLightRTV;
    return true;
}

bool FFogRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    return true;
}

bool FFogRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
