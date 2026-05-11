#pragma once

#include "Viewport/ViewportClient.h"
#include "Render/Types/POVProvider.h"
#include "Render/Types/ViewTypes.h"
#include "Editor/Viewport/ViewportCameraTransform.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "UI/SWindow.h"
#include "Core/RayTypes.h"
#include "Gizmo/BoneTransformGizmoTarget.h"

#include <d3d11.h>

class UGizmoComponent;
class FWindowsWindow;
class UWorld;
class AActor;
class USkeletalMesh;

class FMeshEditorViewportClient : public FViewportClient, public IPOVProvider
{
public:
	void Initialize(ID3D11Device* Device, uint32 Width, uint32 Height);
	void Release();

	void CreatePreviewGizmo();

	void SetPreviewWorld(UWorld* InWorld) { PreviewWorld = InWorld; }
	void SetPreviewActor(AActor* InActor) { PreviewActor = InActor; }
	void SetPreviewMeshComponent(USkeletalMeshComponent* InComp) { PreviewMeshComponent = InComp; }
	void SetViewportRect(float X, float Y, float Width, float Height) { ViewportScreenRect = { X, Y, Width, Height }; }

	bool IsRenderable() const { return bIsRenderable; }
	bool IsMouseOverViewport() const;
	bool IsGizmoHolding() const;

	FViewport* GetViewport() const { return Viewport; }
	UWorld* GetPreviewWorld() const { return PreviewWorld; }
	UGizmoComponent* GetGizmo() const { return Gizmo; }
	USkeletalMeshComponent* GetPreviewMeshComponent() const { return PreviewMeshComponent; }

	void NotifyViewportResized(int32 NewWidth, int32 NewHeight);

	bool GetCameraView(FMinimalViewInfo& OutPOV) const override;

	void Tick(float DeltaTime);

	void SetSelectedBone(USkeletalMesh* Mesh, int32 BoneIndex);
	const FBone* GetSelectedBone() const;

	void ApplyTransformSettingsToGizmo();

private:
	void TickShortcuts();
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void SyncCameraSmoothingTarget();
	void ApplySmoothedCameraLocation(float DeltaTime);

	void SyncGizmo();

	void HandleDragStart(const FRay& Ray);

private:
	USkeletalMesh* SelectedMesh = nullptr;
	int32 SelectedBoneIndex = -1;

	FViewport* Viewport = nullptr;
	FWindowsWindow* Window = nullptr;
	FViewportRenderOptions RenderOptions;

	FBoneTransformGizmoTarget BoneTarget;
	UGizmoComponent* Gizmo = nullptr;
	USkeletalMeshComponent* PreviewMeshComponent = nullptr;

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
