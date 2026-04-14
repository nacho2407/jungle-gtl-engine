#include "RenderPipeline.h"
#include "OpaqueRenderPass.h"

bool FRenderPipeline::Initialize()
{
    OpaqueRenderPass = std::make_shared<FOpaqueRenderPass>();
    OpaqueRenderPass->Initialize();

	RenderPasses.push_back(OpaqueRenderPass);

    return true;
}

bool FRenderPipeline::Render(const FRenderPassContext* Context)
{
	for (std::shared_ptr<FBaseRenderPass> Pass : RenderPasses)
	{
        Pass->Render(Context);
        OutSRV = Pass->GetOutSRV();
	}

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
