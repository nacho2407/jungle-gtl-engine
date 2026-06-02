#pragma once

#include "Viewport/EditorPreviewViewportClient.h"
#include "Viewport/ViewportClient.h"
#include "Editor/Viewport/ViewportCameraTransform.h"
#include "Editor/Slate/SWindow.h"
#include "Core/Types/RayTypes.h"
#include "Gizmo/GizmoTransformTarget.h"
#include "Physics/BodySetup.h"

#include <d3d11.h>

class FViewport;
class UGizmoComponent;
class UPhysicsAsset;
class UPhysicsConstraintTemplate;
class USkeletalMeshComponent;
class UWorld;

class FPhysicsAssetEditorGizmoTarget : public IGizmoTransformTarget
{
public:
	void Clear();
	void SetShape(USkeletalMeshComponent* InMeshComponent, UBodySetup* InBody, EPhysicsAssetShapeType InShapeType, int32 InShapeIndex);
	void SetConstraint(USkeletalMeshComponent* InMeshComponent, UPhysicsConstraintTemplate* InConstraint);
	bool ConsumeDirty();

	bool IsValid() const override;
	UWorld* GetWorld() const override;

	FVector GetWorldLocation() const override;
	FRotator GetWorldRotation() const override;
	FQuat GetWorldQuat() const override;
	FVector GetWorldScale() const override;

	void SetWorldLocation(const FVector& NewLocation) override;
	void SetWorldRotation(const FRotator& NewRotation) override;
	void SetWorldRotation(const FQuat& NewQuat) override;
	void SetWorldScale(const FVector& NewScale) override;

	void AddWorldOffset(const FVector& Delta) override;
	void AddWorldRotation(const FQuat& Delta, bool bWorldSpace) override;
	void AddScaleDelta(const FVector& Delta) override;

private:
	enum class ETargetType : uint8
	{
		None,
		Shape,
		Constraint
	};

	bool GetBodyBoneMatrix(FMatrix& OutMatrix) const;
	bool GetConstraintParentMatrix(FMatrix& OutMatrix) const;
	bool GetShapeLocalTransform(FVector& OutLocation, FQuat& OutRotation) const;
	bool SetShapeLocalLocation(const FVector& NewLocation);
	bool SetShapeLocalRotation(const FQuat& NewRotation);
	FVector GetShapeLocalScale() const;
	bool SetShapeLocalScale(const FVector& NewScale);
	void MarkDirty();

private:
	ETargetType TargetType = ETargetType::None;
	USkeletalMeshComponent* MeshComponent = nullptr;
	UBodySetup* Body = nullptr;
	UPhysicsConstraintTemplate* Constraint = nullptr;
	EPhysicsAssetShapeType ShapeType = EPhysicsAssetShapeType::Sphere;
	int32 ShapeIndex = -1;
	bool bDirty = false;
};

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
	void CreatePreviewGizmo();
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
	void SetSimulatePhysics(bool bInSimulate);
	UGizmoComponent* GetGizmo() const { return Gizmo; }
	bool IsGizmoHolding() const;
	bool IsGizmoHandleAtMouse() const;
	bool ConsumeGizmoEdited();
	void ApplyTransformSettingsToGizmo();
	void SetGizmoBodySelection(int32 BodyIndex);
	void SetGizmoShapeSelection(int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex);
	void SetGizmoConstraintSelection(int32 ConstraintIndex);
	void ClearGizmoSelection();

	bool GetMouseRay(FRay& OutRay) const;
	bool PickBodyShapeAtMouse(FPhysicsAssetEditorHitResult& OutHit) const;
	bool PickConstraintAtMouse(int32& OutConstraintIndex) const;
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
	void TickInteraction(float DeltaTime);
	void SyncCameraSmoothingTarget();
	void ApplySmoothedCameraLocation(float DeltaTime);
	void DrawPreviewSkeleton();
	void DrawPreviewPhysicsAsset();
	void HandleDragStart(const FRay& Ray);

private:
	FViewport* Viewport = nullptr;
	FViewportRenderOptions RenderOptions;
	FViewportCameraTransform ViewTransform;
	FRect ViewportScreenRect;

	UWorld* PreviewWorld = nullptr;
	UPhysicsAsset* PhysicsAsset = nullptr;
	USkeletalMeshComponent* PreviewMeshComponent = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	FPhysicsAssetEditorGizmoTarget GizmoTarget;

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
