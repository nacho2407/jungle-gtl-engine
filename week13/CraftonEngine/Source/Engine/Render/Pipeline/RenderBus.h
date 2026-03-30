#pragma once
#include "Core/CoreTypes.h"
#include "Render/Pipeline/RenderCommand.h"
#include "Render/Types/ViewTypes.h"


class FRenderBus
{
public:
	void Clear();

	// Mesh 패스용 (Opaque, StencilMask, Outline, Gizmo, Translucent)
	void AddCommand(ERenderPass Pass, const FRenderCommand& InCommand);
	void AddCommand(ERenderPass Pass, FRenderCommand&& InCommand);
	const TArray<FRenderCommand>& GetCommands(ERenderPass Pass) const;

	// Batcher 패스용 — 타입 안전한 전용 큐
	void AddFontEntry(FFontEntry&& Entry);
	void AddOverlayFontEntry(FFontEntry&& Entry);
	void AddSubUVEntry(FSubUVEntry&& Entry);
	void AddAABBEntry(FAABBEntry&& Entry);
	void AddGridEntry(FGridEntry&& Entry);

	const TArray<FFontEntry>& GetFontEntries() const { return FontEntries; }
	const TArray<FFontEntry>& GetOverlayFontEntries() const { return OverlayFontEntries; }
	const TArray<FSubUVEntry>& GetSubUVEntries() const { return SubUVEntries; }
	const TArray<FAABBEntry>& GetAABBEntries() const { return AABBEntries; }
	const TArray<FGridEntry>& GetGridEntries() const { return GridEntries; }

	// Getter,Setter
	void SetViewProjection(const FMatrix& InView, const FMatrix& InProj, const FVector& CameraForwardVector, const FVector& CameraRightVector, const FVector& CameraUpVector);
	void SetRenderSettings(const EViewMode NewViewMode, const FShowFlags NewShowFlags);

	const FMatrix& GetView() const { return View; }
	const FMatrix& GetProj() const { return Proj; }
	const FVector& GetCameraForward() const { return CameraForward; }
	const FVector& GetCameraUp() const { return CameraUp; }
	const FVector& GetCameraRight() const { return CameraRight; }
	EViewMode GetViewMode() const { return ViewMode; }
	const FShowFlags& GetShowFlags() const { return ShowFlags; }
	const FVector& GetWireframeColor() const { return WireframeColor; }
	void SetWireframeColor(const FVector& InColor) { WireframeColor = InColor; }

	void SetViewportSize(float InWidth, float InHeight);
	const float GetViewportWidth() const { return viewportWidth; }
	const float GetViewportHeight() const { return viewprotHeight; }

private:
	// Mesh 패스 큐
	TArray<FRenderCommand> PassQueues[(uint32)ERenderPass::MAX];

	// Batcher 패스 큐
	TArray<FFontEntry>  FontEntries;
	TArray<FFontEntry>  OverlayFontEntries;
	TArray<FSubUVEntry> SubUVEntries;
	TArray<FAABBEntry>  AABBEntries;
	TArray<FGridEntry>  GridEntries;

	FMatrix View;
	FMatrix Proj;
	FVector CameraForward;
	FVector CameraRight;
	FVector CameraUp;

	float viewportWidth = 0.0f;
	float viewprotHeight = 0.0f;

	//Editor Settings
	EViewMode ViewMode;
	FShowFlags ShowFlags;
	FVector WireframeColor = FVector(0.0f, 0.0f, 0.7f);
};
