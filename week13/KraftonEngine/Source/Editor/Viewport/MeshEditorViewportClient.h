#pragma once

#include "Viewport/ViewportClient.h"
#include "Render/Types/POVProvider.h"
#include "Editor/Viewport/ViewportCameraTransform.h"

#include "UI/SWindow.h"

#include <d3d11.h>

class UGizmoComponent;
class FWindowsWindow;
class UWorld;
class AActor;

class FMeshEditorViewportClient : public FViewportClient, public IPOVProvider
{
public:
	void Initialize(ID3D11Device* Device, uint32 Width, uint32 Height);
	void Release();

	void SetPreviewWorld(UWorld* InWorld) { PreviewWorld = InWorld; }
	void SetPreviewActor(AActor* InActor) { PreviewActor = InActor; }
	void SetViewportRect(float X, float Y, float Width, float Height) { ViewportScreenRect = { X, Y, Width, Height }; }

	bool IsRenderable() const { return bIsRenderable; }

	FViewport* GetViewport() const { return Viewport; }
	UWorld* GetPreviewWorld() const { return PreviewWorld; }

	bool GetCameraView(FMinimalViewInfo& OutPOV) const override;

	void Tick(float DeltaTime);

private:
	void TickInput(float DeltaTime);
	void SyncCameraSmoothingTarget();
	void ApplySmoothedCameraLocation(float DeltaTime);

private:
	FViewport* Viewport = nullptr;
	FWindowsWindow* Window = nullptr;
	UGizmoComponent* Gizmo = nullptr;

	UWorld* PreviewWorld = nullptr;
	AActor* PreviewActor = nullptr;

	bool bIsRenderable = false;

	FViewportCameraTransform ViewTransform;

	FRect ViewportScreenRect;

	// Camera Smoothing
	FVector TargetLocation;
	bool bTargetLocationInitialized = false;
	FVector LastAppliedCameraLocation;
	bool bLastAppliedCameraLocationInitialized = false;
	const float SmoothLocationSpeed = 10.0f;
};
