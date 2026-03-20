#pragma once

#include <Core/CoreMinimal.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Engine::ApplicationCore
{
    class ENGINE_API FWindowsWindow
    {
      public:
        FWindowsWindow();
        ~FWindowsWindow();

        bool Initialize(const wchar_t* InTitle, int32 InWidth, int32 InHeight);
        void Destroy();

        HWND  GetHWnd() const { return HWnd; }
        int32 GetWidth() const { return Width; }
        int32 GetHeight() const { return Height; }
        float GetAspectRatio() const { return static_cast<float>(Width) / Height; }

        bool IsVisible() const { return bIsVisible; }
        bool IsClosed() const { return bIsClosed; }

      protected:
        static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

      private:
        HWND HWnd = nullptr;

        int32    Width = 0;
        int32    Height = 0;
        FWString Title;

        bool bIsVisible = false;
        bool bIsClosed = false;
        bool bIsFullscreen = false;
    };
}; // namespace Engine::ApplicationCore
