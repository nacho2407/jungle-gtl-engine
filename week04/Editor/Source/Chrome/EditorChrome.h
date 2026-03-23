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
    void SetHost(IEditorChromeHost* InHost) { Host = InHost; }
    void Draw();

  private:
    IEditorChromeHost* Host = nullptr;
};
