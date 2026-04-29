#include "RenderPassPipeline.h"

#include "Render/RenderPass/RenderPassRegistry.h"
#include "Render/Types/RenderTypes.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"

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
		const char* PassName = GetRenderPassName(Pass->GetPassType());

		bool bBegin;
		{
			SCOPE_STAT_CAT(PassName, "3_BeginPass");
			bBegin = Pass->BeginPass(Ctx);
		}
		if (!bBegin) continue;

		{
			SCOPE_STAT_CAT(PassName, "4_ExecutePass");
			GPU_SCOPE_STAT(PassName);
			Pass->Execute(Ctx);
		}

		{
			SCOPE_STAT_CAT(PassName, "5_EndPass");
			Pass->EndPass(Ctx);
		}
	}
}
