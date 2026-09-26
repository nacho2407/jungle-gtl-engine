#pragma once

#include "Core/ViewportClient.h"

class FObjViewerViewportClient final : public FGameViewportClient
{
public:
	FObjViewerViewportClient() = default;
	~FObjViewerViewportClient() override = default;

	void Attach(FEngine* Engine, FRenderer* Renderer) override;
	void Detach(FEngine* Engine, FRenderer* Renderer) override;
	void Tick(FEngine* Engine, float DeltaTime) override;
	void HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) override;

private:
	void LoadDroppedObj(class FObjViewerEngine* Engine, const FString& FilePath);
};
