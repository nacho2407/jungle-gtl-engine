#include "RenderPipeline.h"
#include "OpaqueRenderPass.h"

bool FRenderPipeline::Initialize()
{
    OpaqueRenderPass = std::make_shared<FOpaqueRenderPass>();
    OpaqueRenderPass->Initialize();

    return true;
}

bool FRenderPipeline::Render(const FRenderPassContext* Context)
{
    OpaqueRenderPass->Render(Context);
    return true;
}

void FRenderPipeline::Release()
{
	if (OpaqueRenderPass)
    {
        OpaqueRenderPass->Release();
        OpaqueRenderPass.reset();
	}
}
