#pragma once

#include "Core/Engine.h"
#include "UI/EditorUI.h"
#include "UI/PreviewViewportClient.h"
#include "Controller/EditorViewportController.h"

class AEditorCameraPawn;
class FWindowsWindow;

class FEditorEngine : public FEngine
{
public:
	FEditorEngine() = default;
	~FEditorEngine() override;

	void Shutdown() override;
	void SetSelectedActor(AActor* InActor) override;
	AActor* GetSelectedActor() const override;

protected:
	void PreInitialize() override;
	void OnHostWindowReady(FWindowsWindow* InMainWindow) override;
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
	TObjectPtr<AActor> SelectedActor;
	CEditorViewportController ViewportController;
	FWindowsWindow* HostWindow = nullptr;
};
