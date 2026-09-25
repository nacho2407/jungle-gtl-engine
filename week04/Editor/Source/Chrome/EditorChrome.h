#pragma once

#include "Core/CoreMinimal.h"

#include <functional>

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

    // Win32 hit-test에 쓸 타이틀바 높이와 상호작용 영역을 전달합니다.
    virtual void SetTitleBarMetrics(int32 Height,
                                    const TArray<FEditorChromeRect>& InteractiveRects) = 0;
    // 메인 창을 최소화합니다.
    virtual void MinimizeWindow() = 0;
    // 최대화와 복원을 토글합니다.
    virtual void ToggleMaximizeWindow() = 0;
    // 메인 창 닫기를 요청합니다.
    virtual void CloseWindow() = 0;
    // 현재 메인 창이 최대화 상태인지 조회합니다.
    virtual bool IsWindowMaximized() const = 0;
    // 타이틀바에 표시할 창 제목을 반환합니다.
    virtual const wchar_t* GetWindowTitle() const = 0;
    virtual void* GetNativeWindowHandle() const = 0;
};

enum class EEditorChromeMenuItemType
{
    Action,
    Separator,
    SubMenu
};

struct FEditorChromeMenuItem
{
    EEditorChromeMenuItemType Type = EEditorChromeMenuItemType::Action;
    // 사용자에게 보여 줄 라벨입니다.
    FWString Label;
    // 메뉴 우측에 보일 단축키 텍스트입니다.
    FString ShortcutLabel;
    // 비활성 메뉴로 보여 줄지 결정합니다.
    bool bEnabled = true;
    // 체크 표시가 필요한 메뉴인지 여부입니다.
    bool bCheckable = false;
    // 체크형 메뉴일 때 현재 체크 상태입니다.
    bool bChecked = false;
    // 메뉴를 눌렀을 때 호출할 동작입니다.
    std::function<void()> OnTriggered;
    // submenu 항목일 때 자식 메뉴 목록입니다.
    TArray<FEditorChromeMenuItem> Children;
};

struct FEditorChromeMenu
{
    // 최상위 메뉴에 보일 라벨입니다.
    FWString Label;
    // 이 메뉴 아래에 들어갈 항목 목록입니다.
    TArray<FEditorChromeMenuItem> Items;
};

class FEditorChrome
{
  public:
    // Win32 hit-test와 ImGui 레이아웃이 같은 기준을 쓰도록 상단 바 높이를 한 곳에서 관리합니다.
    static constexpr float TitleBarHeight = 36.0f;

    // 실제 창 제어를 수행하는 host를 연결합니다.
    void SetHost(IEditorChromeHost* InHost) { Host = InHost; }
    // 메뉴, 창 제목, 시스템 버튼을 포함한 상단 chrome 전체를 그립니다.
    void Draw(const TArray<FEditorChromeMenu>& Menus);

  private:
    IEditorChromeHost* Host = nullptr;
};
