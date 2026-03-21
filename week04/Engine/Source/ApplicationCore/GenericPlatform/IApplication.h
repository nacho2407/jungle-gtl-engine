#pragma once
#include <Core/CoreMinimal.h>

namespace Engine::ApplicationCore
{
    class FInputSystem;

    class ENGINE_API IApplication
    {
      public:
        constexpr IApplication() = default;
        virtual ~IApplication() = default;

        virtual void          SetInputSystem(FInputSystem* InInputSystem) = 0;
        virtual FInputSystem* GetInputSystem() const = 0;

        virtual bool CreateApplicationWindow(const wchar_t* InTitle, int32 InWidth,
                                             int32 InHeight) = 0;
        virtual void DestroyApplicationWindow() = 0;

        virtual int32 GetWindowWidth() const = 0;
        
        virtual int32 GetWindowHeight() const = 0;

        virtual void PumpMessages() = 0;

        virtual void ShowWindow() = 0;
        virtual void HideWindow() = 0;

        virtual void* GetNativeWindowHandle() const = 0;

       /* virtual bool ProcessKeyDownEvent(EKey Key, bool bIsRepeat) = 0;
        virtual bool ProcessKeyUpEvent(EKey Key) = 0;

        virtual bool ProcessMouseDownEvent(EKey Button, int32 X, int32 Y) = 0;
        virtual bool ProcessMouseUpEvent(EKey Button, int32 X, int32 Y) = 0;
        virtual bool ProcessMouseDoubleClickEvent(EKey Button, int32 X, int32 Y) = 0;

        virtual bool ProcessMouseMoveEvent(int32 X, int32 Y) = 0;
        virtual bool ProcessRawMouseMoveEvent(int32 DeltaX, int32 DeltaY) = 0;
        virtual bool ProcessMouseWheelEvent(float Delta, int32 X, int32 Y) = 0;*/
    };
} // namespace Engine::ApplicationCore
