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

	// 최종 출력
	OutSRV = OpaqueRenderPass->GetOutSRV();

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
