#include "RenderPassBase.h"

#include "Render/Command/DrawCommandList.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"

void FRenderPassBase::Execute(const FPassContext& Ctx)
{
	uint32 Start, End;
	Ctx.CommandList.GetPassRange(PassType, Start, End);
	if (Start >= End) return;

	const char* PassName = GetRenderPassName(PassType);
	SCOPE_STAT_CAT(PassName, "4_ExecutePass");
	GPU_SCOPE_STAT(PassName);

	Ctx.CommandList.SubmitRange(Start, End, Ctx.Device, Ctx.Resources, Ctx.Cache);
}
