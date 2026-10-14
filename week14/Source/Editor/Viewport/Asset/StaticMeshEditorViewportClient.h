#pragma once

#include "Viewport/EditorPreviewViewportClient.h"
#include "Viewport/ViewportClient.h"
#include "Editor/Viewport/ViewportCameraTransform.h"
#include "Editor/Slate/SWindow.h"

#include <d3d11.h>

class FWindowsWindow;
class UStaticMeshComponent;
class UWorld;
class AActor;

class FStaticMeshEditorViewportClient : public FViewportClient, public IEditorPreviewViewportClient
{
public:
	void Initialize(ID3D11Device* Device, uint32 Width, uint32 Height);
	void Release();

	void ResetCameraToPreviewBounds();

	void SetPreviewWorld(UWorld* InWorld) { PreviewWorld = InWorld; }
	void SetPreviewActor(AActor* InActor) { PreviewActor = InActor; }
	void SetPreviewMeshComponent(UStaticMeshComponent* InComp) { PreviewMeshComponent = InComp; }
	void SetViewportRect(float X, float Y, float Width, float Height) { ViewportScreenRect = { X, Y, Width, Height }; }

	bool IsRenderable() const override { return bIsRenderable; }
	bool IsMouseOverViewport() const override;

	FViewport* GetViewport() const override { return Viewport; }
	UWorld* GetPreviewWorld() const override { return PreviewWorld; }

	FViewportRenderOptions& GetRenderOptions() override { return RenderOptions; }
	const FViewportRenderOptions& GetRenderOptions() const override { return RenderOptions; }

	void NotifyViewportResized(int32 NewWidth, int32 NewHeight) override;
	bool GetCameraView(FMinimalViewInfo& OutPOV) const override;

	void Tick(float DeltaTime);

private:
	void TickShortcuts();
	void TickInput(float DeltaTime);
	void SyncCameraSmoothingTarget();
	void ApplySmoothedCameraLocation(float DeltaTime);

private:
	FViewport* Viewport = nullptr;
	FWindowsWindow* Window = nullptr;
	FViewportRenderOptions RenderOptions;

	UWorld* PreviewWorld = nullptr;
	AActor* PreviewActor = nullptr;
	UStaticMeshComponent* PreviewMeshComponent = nullptr;

	bool bIsRenderable = false;

	FViewportCameraTransform ViewTransform;
	FRect ViewportScreenRect;

	FVector TargetLocation;
	bool bTargetLocationInitialized = false;
	FVector LastAppliedCameraLocation;
	bool bLastAppliedCameraLocationInitialized = false;
	const float SmoothLocationSpeed = 10.0f;
};
