#pragma once
#include "Core/CoreMinimal.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ApplicationCore/Windows/WindowsWindow.h"
#include "ApplicationCore/GenericPlatform/IApplication.h"

class FInputSystem;

namespace Engine::ApplicationCore
{
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
        void DestroyApplicationWindow() override;

        int32 GetWindowWidth() const override { return Window.GetWidth(); }
        int32 GetWindowHeight() const override { return Window.GetHeight(); }

        void PumpMessages() override;
        bool IsExitRequested() const override { return bExitRequested; }

        void ShowWindow() override;
        void HideWindow() override;

        void* GetNativeWindowHandle() const override;

        FWindowsWindow&       GetWindow() { return Window; }
        const FWindowsWindow& GetWindow() const { return Window; }

      private:
        void RegisterRawMouseInput();
        void RequestExit();

      private:
        FWindowsWindow Window;
        FInputSystem*  InputSystem = nullptr;
        bool           bExitRequested = false;
        bool           bRawMouseInputRegistered = false;
        bool           bHasLastMousePosition = false;
        int32          LastMouseX = 0;
        int32          LastMouseY = 0;

        friend LRESULT CALLBACK AppWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam);
    };
} // namespace Engine::ApplicationCore
