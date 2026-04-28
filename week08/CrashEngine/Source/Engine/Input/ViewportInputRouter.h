#pragma once

#include <functional>
#include <windows.h>

#include "Core/CoreTypes.h"

#include "Input/InputTypes.h"

#include "UI/SWindow.h"

class FViewport;
class FViewportClient;

struct FGuiInputCaptureState
{
    bool bMouse = false;
    bool bKeyboard = false;
};

class FViewportInputRouter
{
public:
    // 메인 윈도우에서 타겟 뷰포트의 위치와 크기를 얻어오는 함수 타입
	// FViewport 내에 있는 Rect 정보는 RTV 내에서의 크기를 의미하는, 렌더링에 더 가까운 정보라서 별개임
	// function의 FRect&는 OutRect
    using FRectProvider = std::function<bool(FRect&)>;

    void SetOwnerWindow(HWND InOwnerWindow) { OwnerWindow = InOwnerWindow; }

    void SetGuiCaptureState(const FGuiInputCaptureState& InState)
    {
        GuiCaptureState = InState;
    }

    void ClearTargets();

    void RegisterTarget(
        FViewport* InViewport,
        FViewportClient* InClient,
        FRectProvider InRectProvider);

    void Tick(const FInputSnapshot& Input, float DeltaTime);

    FViewport* GetHoveredViewport() const { return HoveredViewport; }
    FViewport* GetCapturedViewport() const { return CapturedViewport; }
    FViewport* GetFocusedViewport() const { return FocusedViewport; }

private:
    struct FTargetEntry
    {
        FViewport* Viewport = nullptr;
        FViewportClient* Client = nullptr;
        FRectProvider RectProvider;
    };

    static bool IsPointInRect(const POINT& Point, const FRect& Rect);

    FTargetEntry* FindHoveredTarget(const POINT& ClientPos, FRect& OutRect);
    FTargetEntry* FindTargetByViewport(FViewport* InViewport, FRect& OutRect);
    FTargetEntry* ResolvePointerTarget(const POINT& ClientPos, FRect& OutRect);
    FTargetEntry* ResolveKeyTarget(FRect& OutRect);

    void DispatchPointerEvents(FTargetEntry* Target, const FRect& TargetRect, const FInputSnapshot& Input);
    void DispatchAxisEvents(FTargetEntry* Target, const FInputSnapshot& Input, float DeltaTime);
    void DispatchKeyEvents(FTargetEntry* Target, const FInputSnapshot& Input, float DeltaTime);

    POINT ScreenToClientPoint(POINT ScreenPos) const;
    POINT ClientToLocalPoint(const POINT& ClientPos, const FRect& TargetRect) const;

	void ResetKeyRepeatState();

private:
    FGuiInputCaptureState GuiCaptureState{};

    TArray<FTargetEntry> Targets;
    HWND OwnerWindow = nullptr;

    FViewport* HoveredViewport = nullptr;
    FViewport* CapturedViewport = nullptr;
    FViewport* FocusedViewport = nullptr;

	// 키보드 Repeat 처리를 위한 상태 저장용 배열들
	float KeyRepeatElapsed[256] = {}; // Repeat 계산을 위해 누적된 시간
    bool bKeyRepeatActive[256] = {};  // Initial Delay를 넘어 반복 중인 키인지 여부

	// 키보드 Repeat 타이밍 설정
    const float InitialKeyRepeatDelay = 0.35f; // Repeat이 시작되기 전까지의 지연 시간(초)
    const float KeyRepeatInterval = 0.05f; // Repeat이 활성화된 후 키 입력이 반복되는 시간 간격(초)
};
