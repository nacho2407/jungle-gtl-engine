#pragma once
#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/ObjViewerViewportClient.h"
#include "Editor/Settings/ObjViewerSettings.h"
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

	FObjViewerSettings& GetSettings() { return FObjViewerSettings::Get(); }
	const FObjViewerSettings& GetSettings() const { return FObjViewerSettings::Get(); }

private:
	FSelectionManager SelectionManager;
	FObjViewerViewportClient ViewportClient;
};