#include "RenderPassPipeline.h"

#include "Render/RenderPass/RenderPassRegistry.h"
#include "Render/Pipeline/DrawCommandList.h"

void FRenderPassPipeline::Initialize()
{
	Passes = FRenderPassRegistry::Get().CreateAll();

	// 패스 객체로부터 상태 테이블 빌드
	for (const auto& Pass : Passes)
	{
		StateTable.Set(Pass->GetPassType(), Pass->GetRenderState());
	}
}

void FRenderPassPipeline::Execute(const FPassContext& Ctx, FDrawCommandList& CmdList,
                                   FD3DDevice& Device, FSystemResources& Resources)
{
	for (const auto& Pass : Passes)
	{
		Pass->BeginPass(Ctx);
		Pass->Execute(Ctx, CmdList, Device, Resources);
		Pass->EndPass(Ctx);
	}
}
