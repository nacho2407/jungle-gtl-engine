#include "Editor/Viewport/Viewer/ViewerViewportClient.h"

void FViewerViewportClient::SetRealtime(bool bInRealtime)
{
	bRealtime = bInRealtime;
}

void FViewerViewportClient::SetViewMode(EViewMode InViewMode)
{
	if (FEditorViewportState* ViewportState = GetViewportState())
	{
		ViewportState->ViewMode = InViewMode;
	}
}

EViewMode FViewerViewportClient::GetViewMode() const
{
	const FEditorViewportState* ViewportState = GetViewportState();
	return ViewportState ? ViewportState->ViewMode : EViewMode::Lit_BlinnPhong;
}

void FViewerViewportClient::BuildViewerShowFlags(FShowFlags& OutShowFlags) const
{
	OutShowFlags = {};
	OutShowFlags.bPrimitives = true;
	OutShowFlags.bSkeletalMesh = false;
    OutShowFlags.bParticle = false;
	OutShowFlags.bGrid = bShowGrid;
	OutShowFlags.bAxis = bShowAxis;
	OutShowFlags.bGizmo = bShowGizmo;
	OutShowFlags.bBillboardText = false;
	OutShowFlags.bBoundingVolume = bShowBounds;
	OutShowFlags.bBVHBoundingVolume = false;
	OutShowFlags.bEnableLOD = false;
	OutShowFlags.bDecals = false;
	OutShowFlags.bFog = false;
	OutShowFlags.bShadow = false;
	OutShowFlags.bGammaCorrection = false;
}
