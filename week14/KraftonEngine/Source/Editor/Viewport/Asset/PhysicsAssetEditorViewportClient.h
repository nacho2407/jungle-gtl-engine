#pragma once

#include "Viewport/EditorPreviewViewportClient.h"
#include "Viewport/ViewportClient.h"
#include "Editor/Viewport/ViewportCameraTransform.h"
#include "Editor/Slate/SWindow.h"
#include "Core/Types/RayTypes.h"
#include "Physics/BodySetup.h"

#include <d3d11.h>

class FViewport;
class UPhysicsAsset;
class USkeletalMeshComponent;
class UWorld;

struct FPhysicsAssetEditorHitResult
{
	bool bHit = false;
	int32 BodyIndex = -1;
	int32 ShapeIndex = -1;
	EPhysicsAssetShapeType ShapeType = EPhysicsAssetShapeType::Sphere;
	float Distance = 0.0f;
	FVector WorldPosition = FVector::ZeroVector;
};

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
	bool IsShowBodies() const { return RenderOptions.ShowFlags.bPhysicsAssetBodies; }
	void SetShowBodies(bool bInShow) { RenderOptions.ShowFlags.bPhysicsAssetBodies = bInShow; }
	bool IsShowConstraints() const { return RenderOptions.ShowFlags.bPhysicsAssetConstraints; }
	void SetShowConstraints(bool bInShow) { RenderOptions.ShowFlags.bPhysicsAssetConstraints = bInShow; }
	bool IsShowSkeleton() const { return RenderOptions.ShowFlags.bPhysicsAssetSkeleton; }
	void SetShowSkeleton(bool bInShow) { RenderOptions.ShowFlags.bPhysicsAssetSkeleton = bInShow; }
	bool IsSimulatingPhysics() const { return RenderOptions.ShowFlags.bPhysicsAssetSimulation; }
	void SetSimulatePhysics(bool bInSimulate) { RenderOptions.ShowFlags.bPhysicsAssetSimulation = bInSimulate; }

	bool GetMouseRay(FRay& OutRay) const;
	bool PickBodyShapeAtMouse(FPhysicsAssetEditorHitResult& OutHit) const;
	void ClearHighlight();
	void SetHighlightedBody(int32 BodyIndex);
	void SetHighlightedShape(int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex);
	void SetHighlightedConstraint(int32 ConstraintIndex);

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
	void DrawPreviewSkeleton();
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
	int32 HighlightBodyIndex = -1;
	EPhysicsAssetShapeType HighlightShapeType = EPhysicsAssetShapeType::Sphere;
	int32 HighlightShapeIndex = -1;
	int32 HighlightConstraintIndex = -1;

	FVector TargetLocation;
	bool bTargetLocationInitialized = false;
	FVector LastAppliedCameraLocation;
	bool bLastAppliedCameraLocationInitialized = false;
	const float SmoothLocationSpeed = 10.0f;
};
