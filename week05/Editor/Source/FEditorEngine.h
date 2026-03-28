#pragma once

#include "Core/FEngine.h"
#include "UI/EditorUI.h"
#include "Viewport/ViewportTypes.h"
#include "Viewport/Viewport.h"
#include "Viewport/PreviewViewportClient.h"
#include "Controller/EditorViewportController.h"

class AEditorCameraPawn;

class FEditorEngine : public FEngine
{
public:
	FEditorEngine() = default;
	~FEditorEngine() override;

	bool Initialize(HINSTANCE hInstance);
	void Shutdown() override;

	const TArray<FViewport>& GetViewports() const { return Viewports; }
	TArray<FViewport>& GetViewports() { return Viewports; }

protected:
	void PreInitialize() override;
	void PostInitialize() override;
	void Tick(float DeltaTime) override;
	ESceneType GetStartupSceneType() const override { return ESceneType::Editor; }
	std::unique_ptr<IViewportClient> CreateViewportClient() override;

	CEditorViewportController* GetViewportController();
	FViewport* FindViewport(FViewportId Id);
private:
	void SyncViewportClient();
	void InitializeViewportStorage();

	CEditorUI EditorUI;
	std::unique_ptr<CPreviewViewportClient> PreviewViewportClient;
	AEditorCameraPawn* EditorPawn = nullptr;
	CEditorViewportController ViewportController;

	TArray<FViewport> Viewports;
	CEditorViewportClient* EditorViewportClientRaw = nullptr;
};
