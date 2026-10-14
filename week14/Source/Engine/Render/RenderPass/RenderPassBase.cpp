#include "RenderPassBase.h"

#include "Render/Command/DrawCommandList.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"

void FRenderPassBase::Execute(const FPassContext& Ctx)
{
	uint32 Start, End;
	Ctx.CommandList.GetPassRange(PassType, Start, End);
	if (Start >= End) return;

	Ctx.CommandList.SubmitRange(Start, End, Ctx.Device, Ctx.Resources, Ctx.Cache);
}
