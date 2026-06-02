#pragma once

#include "Viewport/EditorPreviewViewportClient.h"
#include "Viewport/ViewportClient.h"
#include "Editor/Viewport/ViewportCameraTransform.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Editor/Slate/SWindow.h"
#include "Core/Types/CoreTypes.h"
#include "Core/Types/RayTypes.h"
#include "Gizmo/BoneTransformGizmoTarget.h"
#include "Gizmo/PhysicsAssetConstraintGizmoTarget.h"
#include "Gizmo/PhysicsAssetShapeGizmoTarget.h"
#include "Component/Debug/BoneDebugComponent.h"
#include "Object/GarbageCollection.h"

#include <d3d11.h>

class UGizmoComponent;
class FWindowsWindow;
class UWorld;
class AActor;
class USkeletalMesh;
class USkeletalMeshComponent;
class USkeletalMeshDebugComponent;
class UPhysicsAsset;
class UPhysicsAssetDebugComponent;

class FMeshEditorViewportClient : public FViewportClient, public IEditorPreviewViewportClient, public FGCObject
{
public:
	void Initialize(ID3D11Device* Device, uint32 Width, uint32 Height);
	void Release();

	const char* GetReferencerName() const override { return "FMeshEditorViewportClient"; }
	void AddReferencedObjects(FReferenceCollector& Collector) override;

	void CreatePreviewGizmo();
	void CreateBoneDebugComponent();
	void CreatePhysicsAssetDebugComponent();
	void ResetCameraToPreviousBounds();

	void SetPreviewWorld(UWorld* InWorld) { PreviewWorld = InWorld; }
	void SetPreviewActor(AActor* InActor) { PreviewActor = InActor; }
	void SetPreviewMeshComponent(USkeletalMeshDebugComponent* InComp);
	void SetViewportRect(float X, float Y, float Width, float Height) { ViewportScreenRect = { X, Y, Width, Height }; }

	bool IsRenderable() const override { return bIsRenderable; }
	bool IsMouseOverViewport() const override;

	bool IsGizmoHolding() const;

	FViewport* GetViewport() const override { return Viewport; }
	UWorld* GetPreviewWorld() const override { return PreviewWorld; }

	UGizmoComponent* GetGizmo() const { return Gizmo; }
	USkeletalMeshComponent* GetPreviewMeshComponent() const { return PreviewMeshComponent; }
	USkeletalMeshDebugComponent* GetPreviewDebugMeshComponent() const { return PreviewDebugMeshComponent; }
	UPhysicsAssetDebugComponent* GetPhysicsAssetDebugComponent() const { return PhysicsAssetDebugComponent; }
	void SyncPhysicsAssetDebugComponent(
		UPhysicsAsset* PhysicsAsset,
		int32 SelectedBodyIndex,
		int32 SelectedConstraintIndex = -1);
	void SetPhysicsAssetPickingEnabled(bool bInEnabled);
	void SetOnPhysicsAssetBodyPicked(TFunction<void(int32)> InCallback);
	void SetOnPhysicsAssetConstraintPicked(TFunction<void(int32)> InCallback);
	void SetOnPhysicsAssetShapeEdited(TFunction<void()> InCallback);
	void SetOnPhysicsAssetConstraintEdited(TFunction<void()> InCallback);

	FViewportRenderOptions& GetRenderOptions() override { return RenderOptions; }
	const FViewportRenderOptions& GetRenderOptions() const override { return RenderOptions; }

	void NotifyViewportResized(int32 NewWidth, int32 NewHeight) override;

	bool GetCameraView(FMinimalViewInfo& OutPOV) const override;

	void Tick(float DeltaTime);

	void SetSelectedBone(USkeletalMesh* Mesh, int32 BoneIndex);
	const FBone* GetSelectedBone() const;

	EBoneDebugDrawMode GetBoneDebugDrawMode() const;
	void SetBoneDebugDrawMode(EBoneDebugDrawMode InDrawMode);

	void ApplyTransformSettingsToGizmo();

private:
	void TickShortcuts();
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void SyncCameraSmoothingTarget();
	void ApplySmoothedCameraLocation(float DeltaTime);

	void SyncGizmo();
	void SyncPhysicsAssetShapeGizmoTarget(UPhysicsAsset* PhysicsAsset, int32 SelectedBodyIndex);
	void SyncPhysicsAssetConstraintGizmoTarget(UPhysicsAsset* PhysicsAsset, int32 SelectedConstraintIndex);

	void HandleDragStart(const FRay& Ray);
	void NotifyPhysicsAssetBodyPicked(int32 BodyIndex);
	void NotifyPhysicsAssetConstraintPicked(int32 ConstraintIndex);
	bool IsPhysicsAssetShapeGizmoActive() const;
	bool IsPhysicsAssetConstraintGizmoActive() const;

private:
	USkeletalMesh* SelectedMesh = nullptr;
	int32 SelectedBoneIndex = -1;

	FViewport* Viewport = nullptr;
	FWindowsWindow* Window = nullptr;
	FViewportRenderOptions RenderOptions;

	FBoneTransformGizmoTarget BoneTarget;
	FPhysicsAssetShapeGizmoTarget PhysicsAssetShapeTarget;
	FPhysicsAssetConstraintGizmoTarget PhysicsAssetConstraintTarget;
	UGizmoComponent* Gizmo = nullptr;
	USkeletalMeshComponent* PreviewMeshComponent = nullptr;
	USkeletalMeshDebugComponent* PreviewDebugMeshComponent = nullptr;
	UBoneDebugComponent* BoneDebugComponent = nullptr;
	UPhysicsAssetDebugComponent* PhysicsAssetDebugComponent = nullptr;
	bool bPhysicsAssetPickingEnabled = false;
	int32 SelectedPhysicsConstraintIndex = -1;
	TFunction<void(int32)> OnPhysicsAssetBodyPicked;
	TFunction<void(int32)> OnPhysicsAssetConstraintPicked;
	TFunction<void()> OnPhysicsAssetShapeEdited;
	TFunction<void()> OnPhysicsAssetConstraintEdited;

	UWorld* PreviewWorld = nullptr;
	AActor* PreviewActor = nullptr;

	bool bIsRenderable = false;

	FViewportCameraTransform ViewTransform;

	FRect ViewportScreenRect;

	// Camera Focus Animation
	bool bIsFocusAnimating = false;
	FVector FocusStartLoc;
	FRotator FocusStartRot;
	FVector FocusEndLoc;
	FRotator FocusEndRot;
	float FocusAnimTimer = 0.0f;
	const float FocusAnimDuration = 0.5f;

	// Camera Smoothing
	FVector TargetLocation;
	bool bTargetLocationInitialized = false;
	FVector LastAppliedCameraLocation;
	bool bLastAppliedCameraLocationInitialized = false;
	const float SmoothLocationSpeed = 10.0f;
};
