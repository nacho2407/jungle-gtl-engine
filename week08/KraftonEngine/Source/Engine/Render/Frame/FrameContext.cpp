#include "FrameContext.h"
#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"

void FFrameContext::SetCameraInfo(const UCameraComponent* Camera)
{
	View            = Camera->GetViewMatrix();
	Proj            = Camera->GetProjectionMatrix();
	CameraPosition  = Camera->GetWorldLocation();
	CameraForward   = Camera->GetForwardVector();
	CameraRight     = Camera->GetRightVector();
	CameraUp        = Camera->GetUpVector();
	bIsOrtho        = Camera->IsOrthogonal();
	OrthoWidth      = Camera->GetOrthoWidth();
	NearClip        = Camera->GetCameraState().NearZ;
	FarClip         = Camera->GetCameraState().FarZ;

	// Per-viewport frustum — used by RenderCollector for inline frustum culling
	FrustumVolume.UpdateFromMatrix(View * Proj);
}

void FFrameContext::SetViewportInfo(const FViewport* VP)
{
	if (!VP)
	{
		ViewportWidth = 0.0f;
		ViewportHeight = 0.0f;
		return;
	}

	ViewportWidth = static_cast<float>(VP->GetWidth());
	ViewportHeight = static_cast<float>(VP->GetHeight());
}
