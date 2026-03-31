#pragma once

#include "Engine/Runtime/Engine.h"
#include "ObjViewer/ObjViewerPanel.h"
#include "ObjViewer/ObjViewerViewportClient.h"

class UObjViewerEngine : public UEngine
{
public:
	DECLARE_CLASS(UObjViewerEngine, UEngine)

	UObjViewerEngine() = default;
	~UObjViewerEngine() override = default;

	// Lifecycle overrides
	void Init(FWindowsWindow* InWindow) override;
	void Shutdown() override;
	void Tick(float DeltaTime) override;

	// 프리뷰 메시 로드 — 패널에서 선택 시 호출
	void LoadPreviewMesh(const FString& MeshPath);

	// 접근자
	FObjViewerViewportClient* GetViewportClient() { return &ViewportClient; }
	void RenderUI(float DeltaTime);

private:
	FObjViewerPanel Panel;
	FObjViewerViewportClient ViewportClient;
};
