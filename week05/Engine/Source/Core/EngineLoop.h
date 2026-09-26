#pragma once

#include "CoreMinimal.h"
#include "Core/Engine.h"
#include "Platform/Windows/WindowsEngineLaunch.h"
#include <memory>

class FWindowsApplication;
class FWindowsWindow;

// 엔진 실행의 최상위 오케스트레이터다.
// 애플리케이션 생성, 엔진 초기화, 프레임 반복, 종료 순서를 한곳에서 관리한다.
class ENGINE_API FEngineLoop
{
public:
	FEngineLoop() = default;
	~FEngineLoop();

	/** 윈도우 애플리케이션과 메인 윈도우를 먼저 준비하고, 사용할 엔진 인스턴스를 만든다. */
	bool PreInit(HINSTANCE hInstance, const FEngineLaunchConfig& InConfig);
	/** 생성된 엔진에 창 핸들과 초기 해상도를 넘겨 실제 런타임 초기화를 수행한다. */
	bool Init();
	/** 메시지 펌프와 엔진 Tick을 한 번 실행하는 메인 루프 본체다. */
	void Tick();
	/** 엔진과 애플리케이션을 역순으로 정리하고 프로세스 종료 직전 상태로 되돌린다. */
	void Exit();

	/** 다음 루프에서 종료되도록 플래그만 세운다. */
	void RequestExit();
	/** 외부 루프가 종료 여부를 판단할 때 사용하는 상태 조회 함수다. */
	bool IsExitRequested() const;

	FEngine* GetEngine() const { return Engine.get(); }
	FWindowsApplication* GetApp() const { return App; }
	FWindowsWindow* GetMainWindow() const { return MainWindow; }

private:
	/** Windows 애플리케이션 싱글턴과 메인 윈도우를 생성한다. */
	bool InitializeApplication(HINSTANCE hInstance);
	/** Launch 설정에 맞는 구체 엔진 클래스를 만든다. */
	bool CreateEngineInstance();
	/** 창 정보를 엔진에 전달하고 메시지/리사이즈 콜백을 연결한다. */
	bool InitializeEngine() const;

private:
	FEngineLaunchConfig Config;
	bool bExitRequested = false;

	// 현재는 Windows 애플리케이션 싱글턴과 메인 윈도우를 직접 보관한다.
	FWindowsApplication* App = nullptr;
	FWindowsWindow* MainWindow = nullptr;
	std::unique_ptr<FEngine> Engine;
};
