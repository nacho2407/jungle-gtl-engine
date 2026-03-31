#pragma once

#include "Core/CoreTypes.h"

class FRenderer;
class FWindowsWindow;
class UObjViewerEngine;

// ObjViewer ImGui 패널 — Obj 목록 + 3D 프리뷰 뷰포트
class FObjViewerPanel
{
public:
	void Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UObjViewerEngine* InEngine);
	void Release();
	void Render(float DeltaTime);
	void Update();

private:
	void RenderMeshList();
	void RenderPreviewViewport(float DeltaTime);

private:
	FWindowsWindow* Window = nullptr;
	UObjViewerEngine* Engine = nullptr;

	int32 SelectedMeshIndex = -1;
};
