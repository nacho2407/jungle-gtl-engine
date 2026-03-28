#pragma once

#include "Core/ViewportClient.h"
#include "Gizmo/Gizmo.h"
#include "Picking/Picker.h"
#include "Types/CoreTypes.h"

class FEditorUI;
class FFrustum;
struct FRenderCommandQueue;

enum class ERenderMode
{
	Lighting,
	NoLighting,
	Wireframe,
};

class FEditorViewportClient : public IViewportClient
{
public:
	explicit FEditorViewportClient(FEditorUI& InEditorUI);

	void Attach(FEngine* Engine, FRenderer* Renderer) override;
	void Detach(FEngine* Engine, FRenderer* Renderer) override;
	void Tick(FEngine* Engine, float DeltaTime) override;
	void HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) override;
	EGizmoMode GetGizmoMode() const { return Gizmo.GetMode(); }
	void SetGizmoMode(EGizmoMode InMode) const { Gizmo.SetMode(InMode); }
	ERenderMode GetRenderMode() const { return RenderMode; }
	void SetRenderMode(ERenderMode InRenderMode) { RenderMode = InRenderMode; }

	void HandleFileDoubleClick(const FString& FilePath) override;
	void HandleFileDropOnViewport(const FString& FilePath) override;
	void BuildRenderCommands(FEngine* Engine, UScene* Scene,
		const FFrustum& Frustum, FRenderCommandQueue& OutQueue) override;
	float GetGridSize() const { return GridSize; }
	void SetGridSize(float InSize);
	float GetLineThickness() const { return LineThickness; }
	void SetLineThickness(float InThickness);
	bool IsGridVisible() const { return bShowGrid; }
	void SetGridVisible(bool bVisible) { bShowGrid = bVisible; }
private:
	FEditorUI& EditorUI;
	FPicker Picker;
	mutable FGizmo Gizmo;

	ERenderMode RenderMode = ERenderMode::Lighting;
	const FString WireframeMaterialName = "M_Wireframe";
	std::shared_ptr<FMaterial> WireFrameMaterial = nullptr;

	int32 ScreenWidth = 0;
	int32 ScreenHeight = 0;
	int32 ScreenMouseX = 0;
	int32 ScreenMouseY = 0;

	// 그리드 렌더링용
	std::unique_ptr<FMeshData> GridMesh;
	std::shared_ptr<FMaterial> GridMaterial;
	void CreateGridResource(FRenderer* Renderer);
	float GridSize = 10.0f;
	float LineThickness = 1.0f;
	bool bShowGrid = true;
};
