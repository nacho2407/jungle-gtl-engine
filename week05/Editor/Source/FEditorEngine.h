#pragma once

#include "Core/FEngine.h"
#include "UI/EditorUI.h"
#include "UI/PreviewViewportClient.h"
#include "Controller/EditorViewportController.h"

class AEditorCameraPawn;

class FEditorEngine : public FEngine
{
public:
	FEditorEngine() = default;
	~FEditorEngine() override;

	bool Initialize(HINSTANCE hInstance);
	void Shutdown() override;

protected:
	void PreInitialize() override;
	void PostInitialize() override;
	void Tick(float DeltaTime) override;
	ESceneType GetStartupSceneType() const override { return ESceneType::Editor; }
	std::unique_ptr<IViewportClient> CreateViewportClient() override;

	CEditorViewportController* GetViewportController();
private:
	void SyncViewportClient();

	CEditorUI EditorUI;
	std::unique_ptr<CPreviewViewportClient> PreviewViewportClient;
	AEditorCameraPawn* EditorPawn = nullptr;
	CEditorViewportController ViewportController;
};
