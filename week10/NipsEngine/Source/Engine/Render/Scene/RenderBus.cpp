#include "RenderBus.h"
#include <UI/EditorConsoleWidget.h>

void FRenderBus::Clear()
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		PassQueues[i].clear();
	}

	Lights.clear();
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

	const FMatrix CameraWorldMatrix = InView.GetInverse();
	CameraPosition = CameraWorldMatrix.GetOrigin();
	CameraForward = CameraWorldMatrix.GetForwardVector().GetSafeNormal();
	CameraRight = CameraWorldMatrix.GetRightVector().GetSafeNormal();
	CameraUp = CameraWorldMatrix.GetUpVector().GetSafeNormal();
}

void FRenderBus::SetRenderSettings(const EViewMode NewViewMode, const FShowFlags NewShowFlags)
{
	ViewMode = NewViewMode;
	ShowFlags = NewShowFlags;
}
