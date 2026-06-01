#pragma once

#include "Viewport/EditorPreviewViewportClient.h"
#include "Viewport/ViewportClient.h"
#include "Editor/Viewport/ViewportCameraTransform.h"
#include "Editor/Slate/SWindow.h"

#include <d3d11.h>

class FViewport;
class UPhysicsAsset;
class USkeletalMeshComponent;
class UWorld;

class FPhysicsAssetEditorViewportClient : public FViewportClient, public IEditorPreviewViewportClient
{
public:
	void Initialize(ID3D11Device* Device, uint32 Width, uint32 Height);
	void Release();

	void SetPreviewScene(UWorld* InWorld, UPhysicsAsset* InAsset, USkeletalMeshComponent* InMeshComponent);
	void SetViewportRect(float X, float Y, float Width, float Height) { ViewportScreenRect = { X, Y, Width, Height }; }
	void ResetCameraToPreviewBounds();

	bool IsShowPreviewMesh() const { return bShowPreviewMesh; }
	void SetShowPreviewMesh(bool bInShow);
	bool IsShowBodies() const { return bShowBodies; }
	void SetShowBodies(bool bInShow) { bShowBodies = bInShow; }
	bool IsShowConstraints() const { return bShowConstraints; }
	void SetShowConstraints(bool bInShow) { bShowConstraints = bInShow; }

	bool IsRenderable() const override { return bIsRenderable; }
	bool IsMouseOverViewport() const override;

	FViewport* GetViewport() const override { return Viewport; }
	UWorld* GetPreviewWorld() const override { return PreviewWorld; }

	FViewportRenderOptions& GetRenderOptions() override { return RenderOptions; }
	const FViewportRenderOptions& GetRenderOptions() const override { return RenderOptions; }

	void NotifyViewportResized(int32 NewWidth, int32 NewHeight) override;
	bool GetCameraView(FMinimalViewInfo& OutPOV) const override;
	void SubmitFrameDebugDraw() override;

	void Tick(float DeltaTime);

private:
	void TickShortcuts();
	void TickInput(float DeltaTime);
	void SyncCameraSmoothingTarget();
	void ApplySmoothedCameraLocation(float DeltaTime);
	void DrawPreviewPhysicsAsset();

private:
	FViewport* Viewport = nullptr;
	FViewportRenderOptions RenderOptions;
	FViewportCameraTransform ViewTransform;
	FRect ViewportScreenRect;

	UWorld* PreviewWorld = nullptr;
	UPhysicsAsset* PhysicsAsset = nullptr;
	USkeletalMeshComponent* PreviewMeshComponent = nullptr;

	bool bIsRenderable = false;
	bool bShowPreviewMesh = true;
	bool bShowBodies = true;
	bool bShowConstraints = true;

	FVector TargetLocation;
	bool bTargetLocationInitialized = false;
	FVector LastAppliedCameraLocation;
	bool bLastAppliedCameraLocationInitialized = false;
	const float SmoothLocationSpeed = 10.0f;
};
