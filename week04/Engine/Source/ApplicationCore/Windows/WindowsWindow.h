#pragma once

#include "Core/CoreMinimal.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Engine::ApplicationCore
{
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
