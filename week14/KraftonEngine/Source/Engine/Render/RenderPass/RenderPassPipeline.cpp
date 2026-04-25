#include "RenderPassPipeline.h"

#include "Render/RenderPass/RenderPassRegistry.h"
#include "Render/Pipeline/DrawCommandList.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
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

void FRenderPassPipeline::Execute(const FPassContext& Ctx, FDrawCommandList& CmdList,
                                   FD3DDevice& Device, FSystemResources& Resources)
{
	for (const auto& Pass : Passes)
	{
		Pass->BeginPass(Ctx);

		uint32 Start, End;
		CmdList.GetPassRange(Pass->GetPassType(), Start, End);
		if (Start < End)
		{
			const char* PassName = GetRenderPassName(Pass->GetPassType());
			SCOPE_STAT_CAT(PassName, "4_ExecutePass");
			GPU_SCOPE_STAT(PassName);

			CmdList.SubmitRange(Start, End, Device, Resources, Ctx.Cache);
		}

		Pass->EndPass(Ctx);
	}
}
