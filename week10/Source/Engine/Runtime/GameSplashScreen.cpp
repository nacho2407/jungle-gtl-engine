#include "Engine/Runtime/GameSplashScreen.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

#pragma comment(lib, "gdiplus.lib")

namespace
{
    constexpr float MinSplashSeconds = 3.0f;
    constexpr float MaxFadeSeconds = 0.65f;

    FString Trim(FString Value)
    {
        const auto IsSpace = [](unsigned char Ch) { return std::isspace(Ch) != 0; };
        const auto Begin = std::find_if_not(Value.begin(), Value.end(), IsSpace);
        const auto End = std::find_if_not(Value.rbegin(), Value.rend(), IsSpace).base();
        if (Begin >= End)
        {
            return {};
        }
        return FString(Begin, End);
    }

    bool IsTruthySection(const FString& Line, const char* Section)
    {
        return Line == FString("[") + Section + "]";
    }

    RECT GetFallbackSplashRect()
    {
        const int ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
        const int ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
        constexpr int FallbackWidth = 1280;
        constexpr int FallbackHeight = 720;
        const int Width = std::min(FallbackWidth, ScreenWidth);
        const int Height = std::min(FallbackHeight, ScreenHeight);
        const int X = (ScreenWidth - Width) / 2;
        const int Y = (ScreenHeight - Height) / 2;
        return RECT{ X, Y, X + Width, Y + Height };
    }

    float GetFadeSeconds(float Duration)
    {
        return std::min(MaxFadeSeconds, Duration * 0.25f);
    }
}

bool FGameSplashScreen::ShowOverWindow(HINSTANCE InInstance, HWND OwnerWindow)
{
    Instance = InInstance;
    Owner = OwnerWindow;
    const FBrandingSettings Settings = LoadBrandingSettings();
    if (Settings.SplashImagePath.empty())
    {
        return false;
    }

    const std::wstring SplashPath = ResolveRuntimePath(Settings.SplashImagePath);
    if (SplashPath.empty() || !std::filesystem::exists(SplashPath))
    {
        UE_LOG_WARNING("[Splash] Splash image not found: %s", Settings.SplashImagePath.c_str());
        return false;
    }

    Gdiplus::GdiplusStartupInput StartupInput;
    if (Gdiplus::GdiplusStartup(&GdiToken, &StartupInput, nullptr) != Gdiplus::Ok)
    {
        UE_LOG_ERROR("[Splash] Failed to initialize GDI+.");
        return false;
    }

    SplashImage = std::make_unique<Gdiplus::Image>(SplashPath.c_str());
    if (!SplashImage || SplashImage->GetLastStatus() != Gdiplus::Ok)
    {
        UE_LOG_WARNING("[Splash] Failed to load splash image.");
        SplashImage.reset();
        Gdiplus::GdiplusShutdown(GdiToken);
        GdiToken = 0;
        return false;
    }

    WNDCLASSW WindowClass = {};
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = StaticWndProc;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    WindowClass.hbrBackground = nullptr;
    WindowClass.lpszClassName = L"JSEngineGameSplashWindow";
    RegisterClassW(&WindowClass);

    RECT TargetRect = {};
    if (!Owner || !GetWindowRect(Owner, &TargetRect))
    {
        TargetRect = GetFallbackSplashRect();
    }

    const int WindowWidth = std::max(320, static_cast<int>(TargetRect.right - TargetRect.left));
    const int WindowHeight = std::max(180, static_cast<int>(TargetRect.bottom - TargetRect.top));

    MinSeconds = std::max(MinSplashSeconds, Settings.SplashMinSeconds);
    Window = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        WindowClass.lpszClassName,
        L"JSEngine Game",
        WS_POPUP,
        TargetRect.left,
        TargetRect.top,
        WindowWidth,
        WindowHeight,
        Owner,
        nullptr,
        Instance,
        this);

    if (!Window)
    {
        SplashImage.reset();
        Gdiplus::GdiplusShutdown(GdiToken);
        GdiToken = 0;
        return false;
    }

    ShowTime = std::chrono::steady_clock::now();
    ShowWindow(Window, SW_SHOWNOACTIVATE);
    SetWindowPos(
        Window,
        HWND_TOPMOST,
        TargetRect.left,
        TargetRect.top,
        WindowWidth,
        WindowHeight,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    RenderLayeredSplash(0.0f);
    StartRenderThread();
    return true;
}

void FGameSplashScreen::Close()
{
    if (!Window)
    {
        return;
    }

    while (true)
    {
        PumpMessages();
        const float Elapsed = GetElapsedSeconds();
        if (Elapsed >= MinSeconds)
        {
            break;
        }
        Sleep(10);
    }

    StopRenderThread();

    const float FadeSeconds = GetFadeSeconds(std::max(MinSplashSeconds, MinSeconds));
    const auto FadeOutStart = std::chrono::steady_clock::now();
    while (FadeSeconds > 0.001f)
    {
        PumpMessages();
        const float Elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - FadeOutStart).count();
        if (Elapsed >= FadeSeconds)
        {
            break;
        }
        const float Opacity = std::clamp(1.0f - (Elapsed / FadeSeconds), 0.0f, 1.0f);
        RenderLayeredSplash(Opacity);
        Sleep(10);
    }
    RenderLayeredSplash(0.0f);

    DestroyWindow(Window);
    Window = nullptr;
    Owner = nullptr;
    SplashImage.reset();

    if (GdiToken != 0)
    {
        Gdiplus::GdiplusShutdown(GdiToken);
        GdiToken = 0;
    }
}

LRESULT CALLBACK FGameSplashScreen::StaticWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
    FGameSplashScreen* Splash = reinterpret_cast<FGameSplashScreen*>(GetWindowLongPtrW(HWnd, GWLP_USERDATA));
    if (Message == WM_NCCREATE)
    {
        CREATESTRUCTW* CreateStruct = reinterpret_cast<CREATESTRUCTW*>(LParam);
        Splash = reinterpret_cast<FGameSplashScreen*>(CreateStruct->lpCreateParams);
        SetWindowLongPtrW(HWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Splash));
        if (Splash)
        {
            Splash->Window = HWnd;
        }
    }

    return Splash ? Splash->WndProc(HWnd, Message, WParam, LParam) : DefWindowProcW(HWnd, Message, WParam, LParam);
}

LRESULT FGameSplashScreen::WndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
    switch (Message)
    {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT PaintStruct = {};
        BeginPaint(HWnd, &PaintStruct);
        EndPaint(HWnd, &PaintStruct);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(HWnd, Message, WParam, LParam);
}

FGameSplashScreen::FBrandingSettings FGameSplashScreen::LoadBrandingSettings() const
{
    FBrandingSettings Settings;
    const std::filesystem::path GameIniPath = std::filesystem::path(FPaths::SettingsDir()) / L"Game.ini";
    std::ifstream File(GameIniPath);
    if (!File.is_open())
    {
        return Settings;
    }

    bool bInBrandingSection = false;
    FString Line;
    while (std::getline(File, Line))
    {
        Line = Trim(Line);
        if (Line.empty() || Line[0] == ';' || Line[0] == '#')
        {
            continue;
        }

        if (Line[0] == '[')
        {
            bInBrandingSection = IsTruthySection(Line, "Branding");
            continue;
        }

        if (!bInBrandingSection)
        {
            continue;
        }

        const size_t Equals = Line.find('=');
        if (Equals == FString::npos)
        {
            continue;
        }

        const FString Key = Trim(Line.substr(0, Equals));
        const FString Value = Trim(Line.substr(Equals + 1));
        if (Key == "Splash")
        {
            Settings.SplashImagePath = Value;
        }
        else if (Key == "SplashMinSeconds")
        {
            try
            {
                Settings.SplashMinSeconds = std::max(MinSplashSeconds, std::stof(Value.empty() ? "0" : Value));
            }
            catch (...)
            {
                Settings.SplashMinSeconds = MinSplashSeconds;
            }
        }
    }
    return Settings;
}

std::wstring FGameSplashScreen::ResolveRuntimePath(const FString& Path) const
{
    if (Path.empty())
    {
        return {};
    }

    std::filesystem::path FilePath(FPaths::ToWide(Path));
    if (FilePath.is_absolute())
    {
        return FilePath.lexically_normal().wstring();
    }
    return std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(Path))).lexically_normal().wstring();
}

float FGameSplashScreen::GetElapsedSeconds() const
{
    if (ShowTime.time_since_epoch().count() == 0)
    {
        return 0.0f;
    }
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - ShowTime).count();
}

float FGameSplashScreen::CalculateFadeInOpacity() const
{
    const float Duration = std::max(MinSplashSeconds, MinSeconds);
    const float FadeSeconds = GetFadeSeconds(Duration);
    const float Elapsed = GetElapsedSeconds();

    if (FadeSeconds <= 0.001f)
    {
        return 1.0f;
    }

    if (Elapsed < FadeSeconds)
    {
        return std::clamp(Elapsed / FadeSeconds, 0.0f, 1.0f);
    }

    return 1.0f;
}

void FGameSplashScreen::RenderLayeredSplash(float LogoOpacity)
{
    if (!Window || !SplashImage)
    {
        return;
    }

    RECT WindowRect = {};
    GetWindowRect(Window, &WindowRect);
    const int Width = WindowRect.right - WindowRect.left;
    const int Height = WindowRect.bottom - WindowRect.top;
    if (Width <= 0 || Height <= 0)
    {
        return;
    }

    HDC ScreenDC = GetDC(nullptr);
    if (!ScreenDC)
    {
        return;
    }

    HDC MemoryDC = CreateCompatibleDC(ScreenDC);
    if (!MemoryDC)
    {
        ReleaseDC(nullptr, ScreenDC);
        return;
    }

    BITMAPINFO BitmapInfo = {};
    BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    BitmapInfo.bmiHeader.biWidth = Width;
    BitmapInfo.bmiHeader.biHeight = -Height;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* Bits = nullptr;
    HBITMAP BackBuffer = CreateDIBSection(ScreenDC, &BitmapInfo, DIB_RGB_COLORS, &Bits, nullptr, 0);
    if (!BackBuffer)
    {
        DeleteDC(MemoryDC);
        ReleaseDC(nullptr, ScreenDC);
        return;
    }

    HGDIOBJ OldBitmap = SelectObject(MemoryDC, BackBuffer);
    if (!OldBitmap)
    {
        DeleteObject(BackBuffer);
        DeleteDC(MemoryDC);
        ReleaseDC(nullptr, ScreenDC);
        return;
    }

    Gdiplus::Graphics Graphics(MemoryDC);
    Graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    Graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    Graphics.Clear(Gdiplus::Color(255, 0, 0, 0));

    const float ImageWidth = static_cast<float>(SplashImage->GetWidth());
    const float ImageHeight = static_cast<float>(SplashImage->GetHeight());
    const float MaxLogoWidth = std::min(720.0f, Width * 0.45f);
    const float MaxLogoHeight = std::min(360.0f, Height * 0.35f);
    const float Scale = std::min(MaxLogoWidth / std::max(1.0f, ImageWidth), MaxLogoHeight / std::max(1.0f, ImageHeight));
    const float DrawWidth = ImageWidth * Scale;
    const float DrawHeight = ImageHeight * Scale;
    const float DrawX = (Width - DrawWidth) * 0.5f;
    const float DrawY = (Height - DrawHeight) * 0.5f;

    const float Opacity = std::clamp(LogoOpacity, 0.0f, 1.0f);
    Gdiplus::ImageAttributes Attributes;
    Gdiplus::ColorMatrix ColorMatrix =
    {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, Opacity, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };
    Attributes.SetColorMatrix(&ColorMatrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);

    Graphics.DrawImage(
        SplashImage.get(),
        Gdiplus::RectF(DrawX, DrawY, DrawWidth, DrawHeight),
        0.0f,
        0.0f,
        ImageWidth,
        ImageHeight,
        Gdiplus::UnitPixel,
        &Attributes);

    POINT ScreenPosition = { WindowRect.left, WindowRect.top };
    SIZE WindowSize = { Width, Height };
    POINT SourcePosition = { 0, 0 };
    BLENDFUNCTION Blend = {};
    Blend.BlendOp = AC_SRC_OVER;
    Blend.SourceConstantAlpha = 255;
    Blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(Window, ScreenDC, &ScreenPosition, &WindowSize, MemoryDC, &SourcePosition, 0, &Blend, ULW_ALPHA);

    SelectObject(MemoryDC, OldBitmap);
    DeleteObject(BackBuffer);
    DeleteDC(MemoryDC);
    ReleaseDC(nullptr, ScreenDC);
}

void FGameSplashScreen::StartRenderThread()
{
    bStopRenderThread = false;
    RenderThread = std::thread([this]()
        {
            while (!bStopRenderThread.load())
            {
                RenderLayeredSplash(CalculateFadeInOpacity());
                Sleep(10);
            }
        });
}

void FGameSplashScreen::StopRenderThread()
{
    bStopRenderThread = true;
    if (RenderThread.joinable())
    {
        RenderThread.join();
    }
}

void FGameSplashScreen::PumpMessages()
{
    MSG Message = {};
    while (PeekMessageW(&Message, Window, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Message);
        DispatchMessageW(&Message);
    }
}
