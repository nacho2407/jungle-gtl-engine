#pragma once

#include "Core/Containers/String.h"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <gdiplus.h>
#include <memory>
#include <thread>

class FGameSplashScreen
{
public:
    bool ShowOverWindow(HINSTANCE InInstance, HWND OwnerWindow);
    void Close();

private:
    struct FBrandingSettings
    {
        FString SplashImagePath;
        float SplashMinSeconds = 1.0f;
    };

    static LRESULT CALLBACK StaticWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam);
    LRESULT WndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam);

    FBrandingSettings LoadBrandingSettings() const;
    std::wstring ResolveRuntimePath(const FString& Path) const;
    float GetElapsedSeconds() const;
    float CalculateFadeInOpacity() const;
    void RenderLayeredSplash(float LogoOpacity);
    void StartRenderThread();
    void StopRenderThread();
    void PumpMessages();

private:
    HINSTANCE Instance = nullptr;
    HWND Window = nullptr;
    HWND Owner = nullptr;
    ULONG_PTR GdiToken = 0;
    std::unique_ptr<Gdiplus::Image> SplashImage;
    std::chrono::steady_clock::time_point ShowTime;
    std::thread RenderThread;
    std::atomic_bool bStopRenderThread = false;
    float MinSeconds = 3.0f;
};
