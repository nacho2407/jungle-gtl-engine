#pragma once
#include <Core/CoreMinimal.h>
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
        virtual ~FWindowsApplication() override;

        static FWindowsApplication* Create();

      public:
        virtual void          SetInputSystem(FInputSystem* InInputSystem) override;
        virtual FInputSystem* GetInputSystem() const override;

        virtual bool CreateApplicationWindow(const wchar_t* InTitle, int32 InWidth,
                                             int32 InHeight) override;
        virtual void DestroyApplicationWindow() override;

        virtual int32 GetWindowWidth() const override { return Window.GetWidth(); }
        virtual int32 GetWindowHeight() const override { return Window.GetHeight(); }

        virtual void PumpMessages() override;

        virtual void ShowWindow() override;
        virtual void HideWindow() override;

        virtual void* GetNativeWindowHandle() const override;

        FWindowsWindow&       GetWindow() { return Window; }
        const FWindowsWindow& GetWindow() const { return Window; }

      private:
        void RegisterRawMouseInput();

      private:
        FWindowsWindow Window;
        FInputSystem*  InputSystem = nullptr;
        bool           bRawMouseInputRegistered = false;
        bool           bHasLastMousePosition = false;
        int32          LastMouseX = 0;
        int32          LastMouseY = 0;
    };
} // namespace Engine::ApplicationCore
