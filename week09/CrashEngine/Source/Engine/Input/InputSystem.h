#pragma once

#include <windows.h>

#include "Core/Singleton.h"

#include "InputTypes.h"

class InputSystem : public TSingleton<InputSystem>
{
public:
    void Tick(bool IsWindowFocused);

    const FInputSnapshot& GetSnapshot() const { return CurrentSnapshot; }

    void AddScrollDelta(int Delta);

private:
    void SampleVirtualKeys();
    void SampleMouse();
    void SampleWheel();
    void SampleGamepads();
    void UpdateModifiers();
    void ClearInputOnFocusLost();

private:
    FInputSnapshot CurrentSnapshot{};
    FInputSnapshot PreviousSnapshot{};

    int PendingWheelDelta = 0;

	// 초기 프레임 / 포커스 복귀 / 입력 모드 전환 직후 등 비정상적인 큰 delta를 방지하기 위한 플래그
	bool bHasMouseSample = false;
};
