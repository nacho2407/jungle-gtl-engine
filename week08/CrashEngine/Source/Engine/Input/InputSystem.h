#pragma once

#include <windows.h>

#include "Core/Singleton.h"

#include "InputTypes.h"

// TODO: 역할 상 ViewportInputRouter에 있는 편이 맞기 때문에 Milestone 2에서 ViewportInputRouter로 옮길 예정
struct FGuiInputState
{
    bool bUsingMouse = false;
    bool bUsingKeyboard = false;
};

class InputSystem : public TSingleton<InputSystem>
{
public:
    // TODO: 마찬가지로 Milestone 2에서 ViewportInputRouter로 옮길 예정
    const FGuiInputState& GetGuiInputState() const { return GuiState; }
    void SetGuiCaptureState(bool bMouse, bool bKeyboard);

    void Initialize(HWND InHWnd) { OwnerHWnd = InHWnd; }

    void Tick();

    const FInputSnapshot& GetSnapshot() const { return CurrentSnapshot; }

    void AddScrollDelta(int Delta);

private:
    void SampleKeyboard();
    void SampleMouse();
    void SampleWheel();
    void UpdateModifiers();
    void ClearInputOnFocusLost();

private:
    HWND OwnerHWnd = nullptr;

    FInputSnapshot CurrentSnapshot{};
    FInputSnapshot PreviousSnapshot{};
    FGuiInputState GuiState{};

    int PendingWheelDelta = 0;

	// 초기 프레임 / 포커스 복귀 / 입력 모드 전환 직후 등 비정상적인 큰 delta를 방지하기 위한 플래그
	bool bHasMouseSample = false;
};
