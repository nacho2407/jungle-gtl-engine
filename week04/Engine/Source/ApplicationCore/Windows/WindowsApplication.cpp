#include <Core/CoreMinimal.h>
#include "WindowsApplication.h"

#include <Core/CoreMinimal.h>
#include <windowsx.h>
#include "WindowsApplication.h"

namespace
{
    FWindowsApplication* GWindowsApplication = nullptr;

    int GetMouseX(LPARAM InLParam) { return static_cast<int32>(GET_X_LPARAM(InLParam)); }
    int GetMouseY(LPARAM InLParam) { return static_cast<int32>(GET_Y_LPARAM(InLParam)); }
} // namespace

LRESULT CALLBACK AppWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
    if (GWindowsApplication != nullptr)
    {
        return GWindowsApplication->ProcessMessage(HWnd, Message, WParam, LParam);
    }
    return DefWindowProcW(HWnd, Message, WParam, LParam);
}

LRESULT FWindowsApplication::ProcessMessage(HWND InHWnd, UINT InMessage, WPARAM InWParam,
                                            LPARAM InLParam)
{
    return DefWindowProcW(InHWnd, InMessage, InWParam, InLParam);
}

namespace Engine::ApplicationCore
{

    /*FWindowsApplication::FWindowsApplication() { GWindowsApplication = this; }
    FWindowsApplication::~FWindowsApplication()
    {
        if (GWindowsApplication == this)
        {
            GWindowsApplication = nullptr;
        }
    }

    FWindowsApplication* FWindowsApplication::Create() {}

    void FWindowsApplication::SetMessageHandler(IApplicationMessageHandler* InMessageHandler) {}
    IApplicationMessageHandler* FWindowsApplication::GetMessageHandler() const {}

    bool FWindowsApplication::CreateApplicationWindow(const wchar_t* InTitle, int32 InWidth,
                                                      int32 InHeight)
    {
    }
    void FWindowsApplication::DestroyApplicationWindow() {}

    void FWindowsApplication::PumpMessage() {}

    void FWindowsApplication::ShowWindow() {}
    void FWindowsApplication::HideWindow() {}

    void* FWindowsApplication::GetNativeWindowHandle() const {}

    bool FWindowsApplication::ProcessKeyDownEvent(EKey Key, bool bIsRepeat) {}
    bool FWindowsApplication::ProcessKeyUpEvent(EKey Key) {}

    bool FWindowsApplication::ProcessMouseDownEvent(EKey Button, int32 X, int32 Y) {}
    bool FWindowsApplication::ProcessMouseUpEvent(EKey Button, int32 X, int32 Y) {}
    bool FWindowsApplication::ProcessMouseDoubleClickEvent(EKey Button, int32 X, int32 Y) {}

    bool FWindowsApplication::ProcessMouseMoveEvent(int32 X, int32 Y) {}
    bool FWindowsApplication::ProcessRawMouseMoveEvent(int32 DeltaX, int32 DeltaY) {}
    bool FWindowsApplication::ProcessMouseWheelEvent(float Delta, int32 X, int32 Y) {}*/

    
} // namespace Engine::ApplicationCore
