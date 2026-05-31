#pragma once

#include "Viewport/EditorPreviewViewportClient.h"
#include "Viewport/ViewportClient.h"
#include "Math/Transform.h"
#include "Core/Types/RayTypes.h"

// Forward declarations of Specialist B and D classes
class UPhysicsAsset;
class USkeletalMeshComponent;
class UWorld;
struct FViewportRenderOptions;

struct FShapeIntersectionResult
{
	bool bHit = false;
	float Distance = FLT_MAX;
	int32 BoneIndex = -1;
	int32 ShapeIndex = -1;
	int32 ShapeType = -1; // 0=Sphere, 1=Box, 2=Sphyl
};

class FPhysicsAssetEditorViewportClient : public FViewportClient, public IEditorPreviewViewportClient
{
public:
	FPhysicsAssetEditorViewportClient();
	virtual ~FPhysicsAssetEditorViewportClient() = default;

	// Viewport Client Overrides
	bool IsRenderable() const override { return bIsRenderable; }
	FViewport* GetViewport() const override { return Viewport; }
	UWorld* GetPreviewWorld() const override { return PreviewWorld; }

	FViewportRenderOptions& GetRenderOptions() override { return RenderOptions; }
	const FViewportRenderOptions& GetRenderOptions() const override { return RenderOptions; }

	void NotifyViewportResized(int32 NewWidth, int32 NewHeight) override;
	bool GetCameraView(FMinimalViewInfo& OutPOV) const override;
	void Tick(float DeltaTime);

	void InitializePreviewScene(UWorld* InWorld, UPhysicsAsset* InAsset, USkeletalMeshComponent* InComponent);
	void SetSimulationEnabled(bool bEnable);
	void SetSelectedBone(int32 BoneIndex) { SelectedBoneIndex = BoneIndex; }
	int32 GetSelectedBone() const { return SelectedBoneIndex; }

	void DrawDebugPhysics(UWorld* World, float DeltaTime);
	void DrawDebugJointsAndLimits(UWorld* World);
	void DrawCollisionExclusionLinkers(UWorld* World);

	FShapeIntersectionResult CastSelectionRay(const FRay& Ray);

	void* GetRenderTargetTexture() const;

private:
	UWorld* PreviewWorld = nullptr;
	UPhysicsAsset* EditedPhysicsAsset = nullptr;
	USkeletalMeshComponent* PreviewMeshComponent = nullptr;

	FViewport* Viewport = nullptr;
	FViewportRenderOptions RenderOptions;

	int32 SelectedBoneIndex = -1;
	bool bIsRenderable = true;
	bool bSimulating = false;
};