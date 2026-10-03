#pragma once
#include "Engine/Runtime/Engine.h"

#include "Misc/ObjViewer/Viewport/ObjViewerViewportClient.h"
#include "Misc/ObjViewer/UI/ObjViewerMainPanel.h"
#include "Misc/ObjViewer/Settings/ObjViewerSettings.h"
#include "Core/ResourceManager.h"
#include "Component/StaticMeshComponent.h"

class UObjViewerEngine : public UEngine
{
public:
	DECLARE_CLASS(UObjViewerEngine, UEngine)

	void Init(FWindowsWindow* InWindow) override;
	void BeginPlay() override;
	void Shutdown() override;
	void Tick(float DeltaTime) override;
	
	FViewportCamera* GetCamera() const { return ViewportClient.GetCamera(); }
	FObjViewerViewportClient& GetViewportClient() { return ViewportClient; }

	void RenderUI(float DeltaTime);
	void OnWindowResized(uint32 Width, uint32 Height) override;
	UStaticMeshComponent* GetPreviewMeshComponent() const { return PreviewMeshComponent; }

	FObjViewerSettings& GetSettings() { return FObjViewerSettings::Get(); }
	const FObjViewerSettings& GetSettings() const { return FObjViewerSettings::Get(); }

private:
	FObjViewerMainPanel MainPanel;
	FObjViewerViewportClient ViewportClient;
	UStaticMeshComponent* PreviewMeshComponent = nullptr;
};