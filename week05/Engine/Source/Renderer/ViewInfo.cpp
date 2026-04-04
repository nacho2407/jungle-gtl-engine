#include "Renderer/ViewInfo.h"

void FViewInfo::Initialize(const FSceneViewFamily& InViewFamily, const FSceneView& InView)
{
	static_cast<FSceneView&>(*this) = InView;
	ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;
	CameraPosition = ViewMatrix.GetInverse().GetTranslation();
	Time = InViewFamily.Time;
	DeltaTime = InViewFamily.DeltaTime;
}

bool FViewInfo::HasShowFlag(EEngineShowFlags InFlag) const
{
	return ShowFlags.HasFlag(InFlag);
}
