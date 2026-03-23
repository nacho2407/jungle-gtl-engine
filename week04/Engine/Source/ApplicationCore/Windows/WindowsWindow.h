#pragma once

#include "Core/CoreMinimal.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Engine::ApplicationCore
{
    // ImGui가 계산한 상호작용 영역을 Win32 hit-test에 넘기기 위한 단순 사각형입니다.
    struct FWindowHitTestRect
    {
        int32 Left = 0;
        int32 Top = 0;
        int32 Right = 0;
        int32 Bottom = 0;

        bool Contains(int32 X, int32 Y) const
        {
            return X >= Left && X < Right && Y >= Top && Y < Bottom;
        }
    };

    // 커스텀 타이틀바의 높이와 버튼/메뉴처럼 드래그에서 제외할 영역을 함께 보관합니다.
    struct FCustomTitleBarState
    {
        int32 TitleBarHeight = 0;
        TArray<FWindowHitTestRect> InteractiveRects;
    };

    class ENGINE_API FWindowsWindow
    {
      public:
        FWindowsWindow() = default;
        ~FWindowsWindow() = default;

        bool Create(HINSTANCE InInstance, const wchar_t* InTitle, int32 InWidth, int32 InHeight);
        // Editor는 기본 caption 대신 직접 그린 상단 바를 쓰므로 별도 생성 경로를 둡니다.
        bool CreateEditorWindow(HINSTANCE InInstance, const wchar_t* InTitle, int32 InWidth,
                                int32 InHeight);
        void Destroy();
        void Show();
        void Hide();
        void Close();
        void Minimize();
        void Maximize();
        void Restore();
        void ToggleMaximize();

        HWND  GetHWnd() const { return HWnd; }
        int32 GetWidth() const { return Width; }
        int32 GetHeight() const { return Height; }
        float GetAspectRatio() const { return static_cast<float>(Width) / Height; }
        const FWString& GetTitle() const { return Title; }
        bool IsWindowMaximized() const;
        bool UsesCustomTitleBar() const { return bUsesCustomTitleBar; }
        const FCustomTitleBarState& GetCustomTitleBarState() const { return CustomTitleBarState; }
        void SetCustomTitleBarState(const FCustomTitleBarState& InState)
        {
            CustomTitleBarState = InState;
        }
        void  MarkClosed()
        {
            HWnd = nullptr;
            Width = 0;
            Height = 0;
            bIsVisible = false;
            bIsClosed = true;
        }
        void  SetSize(int32 InWidth, int32 InHeight)
        {
            Width = InWidth;
            Height = InHeight;
        }
        bool IsVisible() const { return bIsVisible; }
        bool IsClosed() const { return bIsClosed; }

      private:
        bool CreateInternal(HINSTANCE InInstance, const wchar_t* InTitle, int32 InWidth,
                            int32 InHeight, DWORD InWindowStyle, bool bInUseCustomTitleBar);

      private:
        HWND HWnd = nullptr;

        int32    Width = 0;
        int32    Height = 0;
        FWString Title;
        FCustomTitleBarState CustomTitleBarState;

        bool bIsVisible = false;
        bool bIsClosed = false;
        bool bIsFullscreen = false;
        bool bUsesCustomTitleBar = false;
    };
} // namespace Engine::ApplicationCore
