#pragma once
#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"

class UObjViewerEngine : public UEngine
{
public:
	DECLARE_CLASS(UObjViewerEngine, UEngine)

	void Init(FWindowsWindow* InWindow) override;
	void BeginPlay() override;
	void Shutdown() override;
	void Tick(float DeltaTime) override;
	void OnWindowResized(uint32 Width, uint32 Height) override;

private:
	FSelectionManager SelectionManager;
	FEditorViewportClient ViewportClient;
};