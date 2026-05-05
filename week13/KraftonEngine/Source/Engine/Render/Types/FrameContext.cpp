#include "FrameContext.h"
#include "Component/CameraComponent.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Viewport/Viewport.h"

// FMinimalViewInfo (POV 통화) → FrameContext (렌더 통화) 변환.
// 매트릭스 빌드는 FMinimalViewInfo 메서드로 모이고, FrameContext 는 결과만 캐시.
void FFrameContext::SetCameraInfo(const FMinimalViewInfo& POV)
{
	CameraPosition  = POV.Location;
	CameraForward   = POV.Rotation.GetForwardVector();
	CameraRight     = POV.Rotation.GetRightVector();
	CameraUp        = POV.Rotation.GetUpVector();

	View = POV.CalculateViewMatrix();
	Proj = POV.CalculateProjectionMatrix();

	bIsOrtho   = POV.bIsOrtho;
	OrthoWidth = POV.OrthoWidth;
	NearClip   = POV.NearClip;
	FarClip    = POV.FarClip;

	// Per-viewport frustum — used by RenderCollector for inline frustum culling
	FrustumVolume.UpdateFromMatrix(View * Proj);
}

// 컴포넌트 오버로드는 통화로 풀어 새 오버로드에 위임. 점진적 호출부 전환을 위해 유지.
void FFrameContext::SetCameraInfo(const UCameraComponent* Camera)
{
	FMinimalViewInfo POV;
	Camera->GetCameraView(0.0f, POV);
	SetCameraInfo(POV);
}

void FFrameContext::SetViewportInfo(const FViewport* VP)
{
	ViewportWidth    = static_cast<float>(VP->GetWidth());
	ViewportHeight   = static_cast<float>(VP->GetHeight());
	ViewportRTV             = VP->GetRTV();
	ViewportDSV             = VP->GetDSV();
	SceneColorCopySRV       = VP->GetSceneColorCopySRV();
	SceneColorCopyTexture   = VP->GetSceneColorCopyTexture();
	ViewportRenderTexture   = VP->GetRTTexture();
	DepthTexture            = VP->GetDepthTexture();
	DepthCopyTexture        = VP->GetDepthCopyTexture();
	DepthCopySRV            = VP->GetDepthCopySRV();
	StencilCopySRV          = VP->GetStencilCopySRV();
	NormalRTV               = VP->GetNormalRTV();
	NormalSRV               = VP->GetNormalSRV();
	CullingHeatmapRTV       = VP->GetCullingHeatmapRTV();
	CullingHeatmapSRV       = VP->GetCullingHeatmapSRV();
}
