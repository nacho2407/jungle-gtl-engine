//#pragma once
//
//#include "Engine/Runtime/Engine.h"
//#include "Mesh/ObjImporter.h"
//#include "ObjViewer/ObjViewerPanel.h"
//#include "ObjViewer/ObjViewerViewportClient.h"
//
//#include <memory>
//
//class FObjViewerRenderPipeline;
//
//class UObjViewerEngine : public UEngine
//{
//public:
//	DECLARE_CLASS(UObjViewerEngine, UEngine)
//
//	UObjViewerEngine() = default;
//	~UObjViewerEngine() override = default;
//
//	// Lifecycle overrides
//	void Init(FWindowsWindow* InWindow) override;
//	void Shutdown() override;
//	void Tick(float DeltaTime) override;
//	void Render(float DeltaTime) override;
//
//	// 프리뷰 메시 로드 — 패널에서 선택 시 호출
//	void LoadPreviewMesh(const FString& MeshPath);
//
//	// OBJ 파일을 옵션 기반으로 Import 후 프리뷰 로드
//	void ImportObjWithOptions(const FString& ObjPath, const FImportOptions& Options);
//
//	// 접근자
//	FObjViewerViewportClient* GetViewportClient() { return &ViewportClient; }
//	void RenderUI(float DeltaTime);
//
//private:
//	FObjViewerPanel Panel;
//	FObjViewerViewportClient ViewportClient;
//	std::unique_ptr<FObjViewerRenderPipeline> RenderPipeline;
//};
