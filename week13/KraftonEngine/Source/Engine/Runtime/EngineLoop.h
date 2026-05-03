#pragma once

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsApplication.h"
#include "Engine/Profiling/Timer.h"

class FEngineLoop
{
public:
	// 구체 UEngine 서브클래스(UEditorEngine / UGameEngine / UObjViewerEngine 등) 를
	// 직접 알지 않기 위해 외부에서 팩토리를 주입한다. 매크로 기반 분기는 Launch.cpp 에서.
	// nullptr 이면 fallback 으로 UEngine 베이스를 spawn.
	using FCreateEngineFn = UEngine* (*)();
	explicit FEngineLoop(FCreateEngineFn InEngineFactory = nullptr);

	bool Init(HINSTANCE hInstance, int nShowCmd);
	int Run();
	void Shutdown();

private:
	void CreateEngine();

private:
	FWindowsApplication Application;
	FTimer Timer;
	FCreateEngineFn EngineFactory = nullptr;
};
