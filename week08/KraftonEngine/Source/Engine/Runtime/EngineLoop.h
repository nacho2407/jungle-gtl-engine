// 런타임 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsApplication.h"
#include "Engine/Profiling/Timer.h"

// FEngineLoop는 런타임 영역의 핵심 동작을 담당합니다.
class FEngineLoop
{
public:
    bool Init(HINSTANCE hInstance, int nShowCmd);
    int Run();
    void Shutdown();

private:
    void CreateEngine();

private:
    FWindowsApplication Application;
    FTimer Timer;
};
