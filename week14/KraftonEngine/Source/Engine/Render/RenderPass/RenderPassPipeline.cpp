#include "RenderPassPipeline.h"

#include "Render/RenderPass/RenderPassRegistry.h"

void FRenderPassPipeline::Initialize()
{
	Passes = FRenderPassRegistry::Get().CreateAll();

	// 패스 객체로부터 상태 테이블 빌드
	for (const auto& Pass : Passes)
	{
		StateTable.Set(Pass->GetPassType(), Pass->GetRenderState());
	}
}

void FRenderPassPipeline::Execute(const FPassContext& Ctx)
{
	for (const auto& Pass : Passes)
	{
		if (!Pass->BeginPass(Ctx))
			continue;
		Pass->Execute(Ctx);
		Pass->EndPass(Ctx);
	}
}
