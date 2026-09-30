// ObjViewer 영역의 비활성화된 예제 타입을 보관합니다.
// #pragma once
//
// #include "Core/CoreTypes.h"
// #include "Mesh/ObjImporter.h"
//
// class FRenderer;
// class FWindowsWindow;
// class UObjViewerEngine;
//
//// ObjViewer ImGui 패널 — Obj 목록 + 3D 프리뷰 뷰포트
// class FObjViewerPanel
//{
// public:
//	void Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UObjViewerEngine* InEngine);
//	void Release();
//	void Render(float DeltaTime);
//	void Update();
//
// private:
//	void RenderMeshList();
//	void RenderPreviewViewport(float DeltaTime);
//	void RenderImportPopup();
//
// private:
//	FWindowsWindow* Window = nullptr;
//	UObjViewerEngine* Engine = nullptr;
//
//	int32 SelectedMeshIndex = -1;
//	int32 SelectedObjIndex = -1;
//	bool bShowImportPopup = false;
//	FImportOptions PendingImportOptions;
// };
