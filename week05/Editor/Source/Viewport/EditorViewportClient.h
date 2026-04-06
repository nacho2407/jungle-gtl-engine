#pragma once

#include "ViewportTypes.h"
#include "Core/ViewportClient.h"
#include "Gizmo/Gizmo.h"
#include "Picking/Picker.h"
#include "Types/CoreTypes.h"
#include "BlitRenderer.h"
#include "Services/EditorViewportInputService.h"
#include "Services/EditorViewportAssetInteractionService.h"
#include "Services/EditorViewportRenderService.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Core/ShowFlags.h"

class FEditorUI;
class FFrustum;
struct FRenderCommandQueue;
class FEditorEngine;
class FEditorViewportRegistry;

class FEditorViewportClient : public IViewportClient
{
public:
	FEditorViewportClient(
		FEditorEngine& InEditorEngine,
		FEditorUI& InEditorUI,
		FEditorViewportRegistry& InViewportRegistry,
		FWindowsWindow* InMainWindow);

	void Attach(FEngine* Engine, FRenderer* Renderer) override;
	void Detach(FEngine* Engine, FRenderer* Renderer) override;
	void Tick(FEngine* Engine, float DeltaTime) override;
	void HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) override;
	EGizmoMode GetGizmoMode() const { return Gizmo.GetMode(); }
	void SetGizmoMode(EGizmoMode InMode) const { Gizmo.SetMode(InMode); }
	EGizmoCoordinateSpace GetSpaceMode() const { return Gizmo.GetCoordinateSpace(); }
	void SetSpaceMode(EGizmoCoordinateSpace InSpace) const { Gizmo.SetCoordinateSpace(InSpace); }
	ERenderMode GetRenderMode() const { return RenderMode; }
	void SetRenderMode(ERenderMode InRenderMode) { RenderMode = InRenderMode; }

	void HandleFileDoubleClick(const FString& FilePath) override;
	void HandleFileDropOnViewport(const FString& FilePath) override;
	void BuildRenderCommands(FEngine* Engine, UScene* Scene,
	const FFrustum& Frustum, const FShowFlags& Flags, const FVector& CameraPosition, const FMatrix& ProjectionMatrix, FRenderCommandQueue& OutQueue) override;
	void Render(FEngine* Engine, FRenderer* Renderer);

private:
	FEditorUI& EditorUI;
	mutable FGizmo Gizmo;
	void SyncViewportRectsFromDock();

	FWindowsWindow* MainWindow = nullptr;

	FPicker Picker;
	FEditorEngine& EditorEngine;
	FEditorViewportRegistry& ViewportRegistry;
	FEditorViewportInputService InputService;
	FEditorViewportAssetInteractionService AssetInteractionService;
	FEditorViewportRenderService RenderService;

	FBlitRenderer BlitRenderer;

	ERenderMode RenderMode = ERenderMode::Lighting;
	const FString WireframeMaterialName = "M_Wireframe";
	std::shared_ptr<FMaterial> WireFrameMaterial = nullptr;


	// 그리드 렌더링용
	std::unique_ptr<FDynamicMesh> GridMesh;
	std::shared_ptr<FMaterial> GridMaterial;
	void CreateGridResource(FRenderer* Renderer);
};
