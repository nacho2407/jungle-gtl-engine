#pragma once

#include "Core/CoreMinimal.h"

struct FEditorChromeRect
{
    int32 Left = 0;
    int32 Top = 0;
    int32 Right = 0;
    int32 Bottom = 0;
};

class IEditorChromeHost
{
  public:
    virtual ~IEditorChromeHost() = default;

    virtual void SetTitleBarMetrics(int32 Height,
                                    const TArray<FEditorChromeRect>& InteractiveRects) = 0;
    virtual void MinimizeWindow() = 0;
    virtual void ToggleMaximizeWindow() = 0;
    virtual void CloseWindow() = 0;
    virtual bool IsWindowMaximized() const = 0;
    virtual const wchar_t* GetWindowTitle() const = 0;
};

class FEditorChrome
{
  public:
    // 플랫폼 hit-test와 ImGui 레이아웃이 같은 기준을 쓰도록 상단 바 높이를 한 곳에서 관리합니다.
    static constexpr float TitleBarHeight = 36.0f;

    void SetHost(IEditorChromeHost* InHost) { Host = InHost; }
    void Draw();

  private:
    IEditorChromeHost* Host = nullptr;
};
