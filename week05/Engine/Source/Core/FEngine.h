#pragma once
#include "CoreMinimal.h"
#include "Scene/SceneTypes.h"
#include "Windows.h"
#include "Core/EngineRuntime.h"
#include "ViewportClient.h"
#include <memory>

class FWindowsApplication;
class FWindowsWindow;

struct FEngineInitArgs
{
	FWindowsApplication* App = nullptr;
	FWindowsWindow* MainWindow = nullptr;
	HWND Hwnd = nullptr;
	int32 Width = 0;
	int32 Height = 0;
};

class ENGINE_API FEngine
{
public:
	FEngine() = default;
	virtual ~FEngine();

	FEngine(const FEngine&) = delete;
	FEngine& operator=(const FEngine&) = delete;

	bool Initialize(const FEngineInitArgs& Args);
	void TickFrame();
	virtual void Shutdown();

	FEngineRuntime* GetRuntime() const { return Runtime.get(); }
	FWindowsApplication* GetApp() const { return App; }
	FWindowsWindow* GetMainWindow() const { return MainWindow; }

protected:
	virtual void PreInitialize() {}
	virtual void PostInitialize() {}
	virtual void Tick(float DeltaTime) {}
	virtual ESceneType GetStartupSceneType() const { return ESceneType::Game; }
	virtual std::unique_ptr<IViewportClient> CreateViewportClient();

	FWindowsApplication* App = nullptr;
	FWindowsWindow* MainWindow = nullptr;
	std::unique_ptr<FEngineRuntime> Runtime;
	std::unique_ptr<IViewportClient> ViewportClient;

private:
	bool OnInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	void OnResize(int32 Width, int32 Height);
};
