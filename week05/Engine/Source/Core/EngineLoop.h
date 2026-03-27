#pragma once

#include "CoreMinimal.h"
#include "Core/FEngine.h"
#include "Platform/Windows/WindowsEngineLaunch.h"
#include <memory>

class FWindowsApplication;
class FWindowsWindow;

// Launch가 만든 프로세스 위에서 앱/엔진 초기화와 프레임 반복을 관리한다.
class ENGINE_API FEngineLoop
{
public:
	FEngineLoop() = default;
	~FEngineLoop();

	bool PreInit(HINSTANCE hInstance, const FEngineLaunchConfig& InConfig);
	bool Init();
	void Tick();
	void Exit();

	void RequestExit();
	bool IsExitRequested() const;

	FEngine* GetEngine() const { return Engine.get(); }
	FWindowsApplication* GetApp() const { return App; }
	FWindowsWindow* GetMainWindow() const { return MainWindow; }

private:
	bool InitializeApplication(HINSTANCE hInstance);
	bool CreateEngineInstance();
	bool InitializeEngine();

private:
	FEngineLaunchConfig Config;
	bool bExitRequested = false;

	FWindowsApplication* App = nullptr;
	FWindowsWindow* MainWindow = nullptr;
	std::unique_ptr<FEngine> Engine;
};
