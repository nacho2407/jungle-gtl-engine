#pragma once
#include "Core/CoreMinimal.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ApplicationCore/Windows/WindowsWindow.h"
#include "ApplicationCore/GenericPlatform/IApplication.h"

class FInputSystem;

namespace Engine::ApplicationCore
{
    using FWindowsMessageHandler =
        bool (*)(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam, LRESULT& OutResult,
                 void* UserData);

    class ENGINE_API FWindowsApplication : public IApplication
    {
      public:
        FWindowsApplication();
        ~FWindowsApplication() override;

        static FWindowsApplication* Create();

      public:
        void          SetInputSystem(FInputSystem* InInputSystem) override;
        FInputSystem* GetInputSystem() const override;

        bool CreateApplicationWindow(const wchar_t* InTitle, int32 InWidth,
                                     int32 InHeight) override;
        bool CreateEditorWindow(const wchar_t* InTitle, int32 InWidth, int32 InHeight);
        void DestroyApplicationWindow() override;

        int32 GetWindowWidth() const override { return Window.GetWidth(); }
        int32 GetWindowHeight() const override { return Window.GetHeight(); }

        void OnResizeWindow(int32 NewWidth, int32 NewHeight);
        void OnFocusGained();
        void OnFocusLost();

        void PumpMessages() override;
        bool IsExitRequested() const override { return bExitRequested; }

        void ShowWindow() override;
        void HideWindow() override;

        void* GetNativeWindowHandle() const override;

        void SetMessageHandler(FWindowsMessageHandler InMessageHandler, void* InUserData = nullptr);
        void SetCustomTitleBarState(const FCustomTitleBarState& InState);
        void MinimizeWindow();
        void ToggleMaximizeWindow();
        void CloseWindow();
        bool IsWindowMaximized() const;
        const wchar_t* GetWindowTitle() const;

        FWindowsWindow&       GetWindow() { return Window; }
        const FWindowsWindow& GetWindow() const { return Window; }

      private:
        bool HandleCustomChromeMessage(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam,
                                       LRESULT& OutResult);
        bool DispatchMessageHandler(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam,
                                    LRESULT& OutResult);
        LRESULT HitTestCustomChrome(HWND HWnd, LPARAM LParam) const;
        void UpdateMaximizedBounds(HWND HWnd, MINMAXINFO* InOutMinMaxInfo) const;
        void RegisterRawMouseInput();
        void RequestExit();

      private:
        FWindowsWindow Window;
        FInputSystem*  InputSystem = nullptr;
        FWindowsMessageHandler MessageHandler = nullptr;
        void* MessageHandlerUserData = nullptr;
        bool           bExitRequested = false;
        bool           bRawMouseInputRegistered = false;
        bool           bHasLastMousePosition = false;
        int32          LastMouseX = 0;
        int32          LastMouseY = 0;

        friend LRESULT CALLBACK AppWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam);
    };
} // namespace Engine::ApplicationCore
