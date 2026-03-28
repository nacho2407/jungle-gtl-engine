#pragma once

#include "CoreMinimal.h"
#include "Core/Engine.h"
#include "Platform/Windows/WindowsEngineLaunch.h"
#include <memory>

class FWindowsApplication;
class FWindowsWindow;

// FEngineLoop는 앱 시작, 엔진 초기화, 프레임 반복, 종료 순서를 관리한다.
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
	bool InitializeEngine() const;

private:
	FEngineLaunchConfig Config;
	bool bExitRequested = false;

	// 현재는 Windows 전용 앱과 윈도우를 직접 사용한다.
	FWindowsApplication* App = nullptr;
	FWindowsWindow* MainWindow = nullptr;
	std::unique_ptr<FEngine> Engine;
};
