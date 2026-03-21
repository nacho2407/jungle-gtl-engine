#pragma once
#include <Core/CoreMinimal.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ApplicationCore/Windows/WindowsWindow.h"
#include "ApplicationCore/GenericPlatform/IApplication.h"
// #include "ApplicationCore/GenericPlatform/IApplicationMessageHandler.h"

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

        /* virtual bool ProcessKeyDownEvent(EKey Key, bool bIsRepeat) override;
         virtual bool ProcessKeyUpEvent(EKey Key) override;

         virtual bool ProcessMouseDownEvent(EKey Button, int32 X, int32 Y) override;
         virtual bool ProcessMouseUpEvent(EKey Button, int32 X, int32 Y) override;
         virtual bool ProcessMouseDoubleClickEvent(EKey Button, int32 X, int32 Y) override;

         virtual bool ProcessMouseMoveEvent(int32 X, int32 Y) override;
         virtual bool ProcessRawMouseMoveEvent(int32 DeltaX, int32 DeltaY) override;
         virtual bool ProcessMouseWheelEvent(float Delta, int32 X, int32 Y) override;

         LRESULT ProcessMessage(HWND InHWnd, UINT InMessage, WPARAM InWParam, LPARAM InLParam);*/

        FWindowsWindow&       GetWindow() { return Window; }
        const FWindowsWindow& GetWindow() const { return Window; }

      private:
        /*EKey TranslateKey(WPARAM InWParam) const;
        EKey TranslateMouseButton(UINT InMessage) const;*/
        void RegisterRawMouseInput();
        void RequestExit();

      private:
        // IApplicationMessageHandler* MessageHandler = nullptr;
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
