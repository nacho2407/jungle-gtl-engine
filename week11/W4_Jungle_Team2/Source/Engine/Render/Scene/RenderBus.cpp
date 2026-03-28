#include "RenderBus.h"

void FRenderBus::Clear()
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		PassQueues[i].clear();
	}
}

void FRenderBus::AddCommand(ERenderPass Pass, const FRenderCommand& InCommand)
{
	PassQueues[(uint32)Pass].push_back(InCommand);
}

void FRenderBus::AddCommand(ERenderPass Pass, FRenderCommand&& InCommand)
{
	PassQueues[(uint32)Pass].push_back(std::move(InCommand));
}

const TArray<FRenderCommand>& FRenderBus::GetCommands(ERenderPass Pass) const
{
	return PassQueues[(uint32)Pass];
}

void FRenderBus::SetViewProjection(const FMatrix& InView, const FMatrix& InProj)
{
	View = InView;
	Proj = InProj;

	CameraRight    = FVector(InView.M[0][0], InView.M[1][0], InView.M[2][0]);
	CameraUp       = FVector(InView.M[0][1], InView.M[1][1], InView.M[2][1]);
	CameraPosition = InView.GetInverse().GetOrigin();
}

void FRenderBus::SetRenderSettings(const EViewMode NewViewMode, const FShowFlags NewShowFlags)
{
	ViewMode = NewViewMode;
	ShowFlags = NewShowFlags;
}
