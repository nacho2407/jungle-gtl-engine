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
}

void FFrameContext::SetViewportInfo(const FViewport* VP)
{
	ViewportWidth    = static_cast<float>(VP->GetWidth());
	ViewportHeight   = static_cast<float>(VP->GetHeight());
	ViewportRTV      = VP->GetRTV();
	ViewportDSV      = VP->GetDSV();
	ViewportDepthSRV = VP->GetDepthSRV();
	ViewportStencilSRV = VP->GetStencilSRV();
}
