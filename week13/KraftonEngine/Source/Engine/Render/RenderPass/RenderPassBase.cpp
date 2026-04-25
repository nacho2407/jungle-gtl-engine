#include "RenderPassBase.h"

#include "Render/Pipeline/DrawCommandList.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"

void FRenderPassBase::Execute(const FPassContext& Ctx, FDrawCommandList& CmdList,
                               FD3DDevice& Device, FSystemResources& Resources)
{
	uint32 Start, End;
	CmdList.GetPassRange(PassType, Start, End);
	if (Start >= End) return;

	const char* PassName = GetRenderPassName(PassType);
	SCOPE_STAT_CAT(PassName, "4_ExecutePass");
	GPU_SCOPE_STAT(PassName);

	CmdList.SubmitRange(Start, End, Device, Resources, Ctx.Cache);
}
